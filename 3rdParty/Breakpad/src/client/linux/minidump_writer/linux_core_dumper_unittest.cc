// Copyright 2012 Google LLC
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// linux_core_dumper_unittest.cc:
// Unit tests for google_breakpad::LinuxCoreDumoer.

#ifdef HAVE_CONFIG_H
#include <config.h>  // Must come first
#endif

#include <string.h>

#include <string>

#include "breakpad_googletest_includes.h"
#include "client/linux/minidump_writer/linux_core_dumper.h"
#include "common/linux/tests/crash_generator.h"
#include "common/tests/auto_tempdir.h"
#include "common/tests/file_utils.h"

namespace google_breakpad {

class LinuxCoreDumperTest : public testing::Test {
 protected:
  AutoTempDir temp_dir;
};

namespace {

// Widens the protected mapping enumeration for tests. Keeps the test-only
// access out of the production headers.
class TestableCoreDumper : public LinuxCoreDumper {
 public:
  using LinuxCoreDumper::LinuxCoreDumper;
  using LinuxCoreDumper::EnumerateMappings;

  // EnumerateMappings() promotes the module holding AT_ENTRY to index 0.
  // ReadAuxv() is not run by these tests, so seed the value directly.
  void SetEntryPoint(uintptr_t entry) { auxv_[AT_ENTRY] = entry; }
};

// Mirrors MinidumpWriter::ShouldIncludeMapping: which mappings become modules.
bool Includable(const MappingInfo* m) {
  if (m->name[0] == '\0' || (m->offset != 0 && !m->exec) || m->size < 4096)
    return false;
  // A later mapping of a split image; the header mapping represents it.
  return !m->split_image.follower;
}

// The size FillRawModule() would emit for |m|.
uintptr_t EmittedSize(const MappingInfo* m) {
  return m->split_image.end_addr ? m->split_image.end_addr - m->start_addr : m->size;
}

// Returns the first mapping named |name| that would be emitted as a module.
const MappingInfo* FindModuleByName(TestableCoreDumper& dumper,
                                    const char* name) {
  for (size_t i = 0; i < dumper.mappings().size(); ++i) {
    if (Includable(dumper.mappings()[i]) &&
        strcmp(dumper.mappings()[i]->name, name) == 0)
      return dumper.mappings()[i];
  }
  return nullptr;
}

// One ELF must produce exactly one module, whatever its segment layout.
size_t CountModulesByName(TestableCoreDumper& dumper, const char* name) {
  size_t count = 0;
  for (size_t i = 0; i < dumper.mappings().size(); ++i) {
    if (Includable(dumper.mappings()[i]) &&
        strcmp(dumper.mappings()[i]->name, name) == 0)
      ++count;
  }
  return count;
}

// Returns the mapping that literally covers |addr|, emitted or not. The
// lookup ranges must keep describing real memory.
const MappingInfo* MappingCovering(TestableCoreDumper& dumper, uintptr_t addr) {
  for (size_t i = 0; i < dumper.mappings().size(); ++i) {
    const MappingInfo* m = dumper.mappings()[i];
    if (addr >= m->start_addr && addr - m->start_addr < m->size)
      return m;
  }
  return nullptr;
}

}  // namespace

TEST_F(LinuxCoreDumperTest, GetMappingAbsolutePath) {
  const LinuxCoreDumper dumper(getpid(), "core", "/tmp", "/mnt/root");
  const MappingInfo mapping = {0, 0, {0, 0}, {0, false}, 0, false,
                               "/usr/lib/libc.so"};

  char path[PATH_MAX];
  dumper.GetMappingAbsolutePath(mapping, path);

  EXPECT_STREQ("/mnt/root/usr/lib/libc.so", path);
}

TEST_F(LinuxCoreDumperTest, BuildProcPath) {
  const pid_t pid = getpid();
  const char procfs_path[] = "/procfs_copy";
  LinuxCoreDumper dumper(getpid(), "core_file", procfs_path);

  char maps_path[NAME_MAX] = "";
  char maps_path_expected[NAME_MAX];
  snprintf(maps_path_expected, sizeof(maps_path_expected),
           "%s/maps", procfs_path);
  EXPECT_TRUE(dumper.BuildProcPath(maps_path, pid, "maps"));
  EXPECT_STREQ(maps_path_expected, maps_path);

  EXPECT_FALSE(dumper.BuildProcPath(nullptr, pid, "maps"));
  EXPECT_FALSE(dumper.BuildProcPath(maps_path, pid, ""));
  EXPECT_FALSE(dumper.BuildProcPath(maps_path, pid, nullptr));

  char long_node[NAME_MAX];
  size_t long_node_len = NAME_MAX - strlen(procfs_path) - 1;
  memset(long_node, 'a', long_node_len);
  long_node[long_node_len] = '\0';
  EXPECT_FALSE(dumper.BuildProcPath(maps_path, pid, long_node));
}

TEST_F(LinuxCoreDumperTest, VerifyDumpWithMultipleThreads) {
  CrashGenerator crash_generator;
  if (!crash_generator.HasDefaultCorePattern()) {
    fprintf(stderr, "LinuxCoreDumperTest.VerifyDumpWithMultipleThreads test "
            "is skipped due to non-default core pattern\n");
    return;
  }

  const unsigned kNumOfThreads = 3;
  const unsigned kCrashThread = 1;
  const int kCrashSignal = SIGABRT;
  pid_t child_pid;
  ASSERT_TRUE(crash_generator.CreateChildCrash(kNumOfThreads, kCrashThread,
                                               kCrashSignal, &child_pid));

  const std::string core_file = crash_generator.GetCoreFilePath();
  const std::string procfs_path = crash_generator.GetDirectoryOfProcFilesCopy();

#if defined(__ANDROID__)
  struct stat st;
  if (stat(core_file.c_str(), &st) != 0) {
    fprintf(stderr, "LinuxCoreDumperTest.VerifyDumpWithMultipleThreads test is "
            "skipped due to no core file being generated\n");
    return;
  }
#endif

  LinuxCoreDumper dumper(child_pid, core_file.c_str(), procfs_path.c_str());

  EXPECT_TRUE(dumper.Init());

  EXPECT_TRUE(dumper.IsPostMortem());

  // These are no-ops and should always return true.
  EXPECT_TRUE(dumper.ThreadsSuspend());
  EXPECT_TRUE(dumper.ThreadsResume());

  // Linux does not set the crash address with SIGABRT, so make sure it always
  // sets the crash address to 0.
  EXPECT_EQ(0U, dumper.crash_address());
  EXPECT_EQ(kCrashSignal, dumper.crash_signal());
  EXPECT_EQ(crash_generator.GetThreadId(kCrashThread),
            dumper.crash_thread());

#if defined(THREAD_SANITIZER)
  EXPECT_GE(dumper.threads().size(), kNumOfThreads);
#else
  EXPECT_EQ(dumper.threads().size(), kNumOfThreads);
#endif
  for (unsigned i = 0; i < kNumOfThreads; ++i) {
    ThreadInfo info;
    EXPECT_TRUE(dumper.GetThreadInfoByIndex(i, &info));
    const void* stack;
    size_t stack_len;
    EXPECT_TRUE(dumper.GetStackInfo(&stack, &stack_len, info.stack_pointer));
    EXPECT_EQ(getpid(), info.ppid);
  }
}

TEST_F(LinuxCoreDumperTest, VerifyExceptionDetails) {
  CrashGenerator crash_generator;
  if (!crash_generator.HasDefaultCorePattern()) {
    fprintf(stderr, "LinuxCoreDumperTest.VerifyDumpWithMultipleThreads test "
            "is skipped due to non-default core pattern\n");
    return;
  }

#ifndef si_syscall
  fprintf(stderr, "LinuxCoreDumperTest.VerifyDumpWithMultipleThreads test is "
          "skipped due to old kernel/C library headers\n");
  return;
#endif

  const unsigned kNumOfThreads = 2;
  const unsigned kCrashThread = 1;
  const int kCrashSignal = SIGSYS;
  pid_t child_pid;
  ASSERT_TRUE(crash_generator.CreateChildCrash(kNumOfThreads, kCrashThread,
                                               kCrashSignal, &child_pid));

  const std::string core_file = crash_generator.GetCoreFilePath();
  const std::string procfs_path = crash_generator.GetDirectoryOfProcFilesCopy();

#if defined(__ANDROID__)
  struct stat st;
  if (stat(core_file.c_str(), &st) != 0) {
    fprintf(stderr, "LinuxCoreDumperTest.VerifyExceptionDetails test is "
            "skipped due to no core file being generated\n");
    return;
  }
#endif

  LinuxCoreDumper dumper(child_pid, core_file.c_str(), procfs_path.c_str());

  EXPECT_TRUE(dumper.Init());

  EXPECT_TRUE(dumper.IsPostMortem());

#if defined(__ANDROID__)
  // TODO: For some reason, Android doesn't seem to pass this.
  if (!dumper.crash_address()) {
    fprintf(stderr, "LinuxCoreDumperTest.VerifyExceptionDetails test is "
            "skipped due to missing signal details on Android\n");
    return;
  }
#endif

  // Check the exception details.
  EXPECT_NE(0U, dumper.crash_address());
  EXPECT_EQ(kCrashSignal, dumper.crash_signal());
  EXPECT_EQ(crash_generator.GetThreadId(kCrashThread),
            dumper.crash_thread());

  // We check the length, but not the actual fields.  We sent SIGSYS ourselves
  // instead of the kernel, so the extended fields are garbage.
  const std::vector<uint64_t> info(dumper.crash_exception_info());
  EXPECT_EQ(2U, info.size());
}

TEST_F(LinuxCoreDumperTest, EnumerateMappings) {
  const char proc_maps_content[] =
      "00000000-00000001 r--p 00000000 00:00 0    /app/libfoo.so\n"
      "00000002-00000004 r-xp 00000000 00:00 0    /app/libfoo.so\n"
      "00000004-00000005 r--p 00000000 00:00 0    /app/libfoo.so\n"
      "00000005-00000006 rw-p 00000000 00:00 0    /app/libfoo.so\n"
      "00000006-00000007 rw-p 00000000 00:00 0    [anno]\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());

  EXPECT_TRUE(dumper.EnumerateMappings());
  // no merge due to the address is not continuous
  EXPECT_EQ(4U, dumper.mappings().size());
  EXPECT_FALSE(dumper.mappings()[0]->exec);
  EXPECT_EQ(1U, dumper.mappings()[0]->size);
}

TEST_F(LinuxCoreDumperTest, EnumerateMappings_diffname) {
  const char proc_maps_content[] =
      "00000000-00000001 r--p 00000000 00:00 0    /app/libfoo.so\n"
      "00000001-00000002 r-xp 00000000 00:00 0    /app/libbar.so\n"
      "00000002-00000003 r--p 00000000 00:00 0    /app/libbar.so\n"
      "00000003-00000004 rw-p 00000000 00:00 0    /app/libbar.so\n"
      "00000004-00000005 rw-p 00000000 00:00 0    [anno]\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());

  EXPECT_TRUE(dumper.EnumerateMappings());
  // no merge due to the lib names are different
  EXPECT_EQ(4U, dumper.mappings().size());
  EXPECT_FALSE(dumper.mappings()[0]->exec);
  EXPECT_EQ(1U, dumper.mappings()[0]->size);
  EXPECT_STREQ("/app/libfoo.so", dumper.mappings()[0]->name);
  EXPECT_STREQ("/app/libbar.so", dumper.mappings()[1]->name);
}

TEST_F(LinuxCoreDumperTest, EnumerateMappings_merge) {
  const char proc_maps_content[] =
      "00000000-00000001 r--p 00000000 00:00 0    /app/libfoo.so\n"
      "00000001-00000002 r-xp 00000000 00:00 0    /app/libfoo.so\n"
      "00000002-00000003 r--p 00000000 00:00 0    /app/libfoo.so\n"
      "00000003-00000004 rw-p 00000000 00:00 0    /app/libfoo.so\n"
      "00000004-00000005 rw-p 00000000 00:00 0    [anno]\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());

  EXPECT_TRUE(dumper.EnumerateMappings());
  EXPECT_EQ(3U, dumper.mappings().size());
  EXPECT_TRUE(dumper.mappings()[0]->exec);
  // merged #1 and #2 in proc_maps_content
  EXPECT_EQ(2U, dumper.mappings()[0]->size);
}

TEST_F(LinuxCoreDumperTest, EnumerateMappings_16K_padding) {
  const char proc_maps_content[] =
      "00000000-00000001 r--p 00000000 00:00 0    /app/libfoo.so\n"
      "00000001-00000002 ---p 00000000 00:00 0    \n"
      "00000002-00000003 r-xp 00000000 00:00 0    /app/libfoo.so\n"
      "00000003-00000004 ---p 00000000 00:00 0    \n"
      "00000004-00000005 r--p 00000000 00:00 0    /app/libfoo.so\n"
      "00000005-00000006 ---p 00000000 00:00 0    \n"
      "00000006-00000007 rw-p 00000000 00:00 0    /app/libfoo.so\n"
      "00000007-00000008 rw-p 00000000 00:00 0    [anno]\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());

  EXPECT_TRUE(dumper.EnumerateMappings());
  EXPECT_EQ(3U, dumper.mappings().size());

  EXPECT_STREQ("/app/libfoo.so", dumper.mappings()[0]->name);
  EXPECT_TRUE(dumper.mappings()[0]->exec);
  EXPECT_FALSE(dumper.mappings()[2]->exec);
  // merged #[1-4] in proc_maps_content
  EXPECT_EQ(4U, dumper.mappings()[0]->size);
}


// The kernel's ELF loader (fs/binfmt_elf.c) unmaps the alignment holes
// between the PT_LOADs of the main executable, so the segments are neither
// adjacent nor separated by a ---p padding entry. They must still form one
// module rooted at the ELF header: any other base makes the file offset of
// the module non-zero, which defeats the build id lookup in
// LinuxDumper::ElfFileIdentifierForMapping().
//
// Layout taken from a real crash, with the high address bits dropped so the
// test also builds for 32-bit. Deltas are unchanged.
TEST_F(LinuxCoreDumperTest, EnumerateMappings_kernel_loader_holes) {
  const char proc_maps_content[] =
      "50c40000-50ca2000 r--p 00000000 fe:07 85 /odm/bin/hw/service\n"
      "50cb0000-50e21000 r-xp 00070000 fe:07 85 /odm/bin/hw/service\n"
      "50e30000-50e3d000 r--p 001f0000 fe:07 85 /odm/bin/hw/service\n"
      "50e4c000-50e50000 rw-p 001fc000 fe:07 85 /odm/bin/hw/service\n"
      "50e50000-50e51000 rw-p 00000000 00:00 0 [anon:.bss]\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());

  ASSERT_TRUE(dumper.EnumerateMappings());

  // Today the executable segment forms its own module at offset 0x70000,
  // which has no ELF header and so cannot be identified.
  EXPECT_EQ(1U, CountModulesByName(dumper, "/odm/bin/hw/service"));

  const MappingInfo* module =
      FindModuleByName(dumper, "/odm/bin/hw/service");
  ASSERT_NE(nullptr, module);
  // Rooted at the ELF header, so the identifier is read from file offset 0.
  EXPECT_EQ(0x50c40000U, module->start_addr);
  EXPECT_EQ(0U, module->offset);
  // Spans the read-only and the executable segment.
  EXPECT_EQ(0x1e1000U, EmittedSize(module));

  // The executable segment keeps its own record and its own true range, so
  // address lookups are unaffected.
  const MappingInfo* code = MappingCovering(dumper, 0x50cb1000);
  ASSERT_NE(nullptr, code);
  EXPECT_EQ(0x50cb0000U, code->start_addr);
  EXPECT_EQ(0x171000U, code->size);
}

// Same defect for the ELF interpreter, which the kernel also maps. Here
// unrelated anonymous mappings sit between the two named segments, so the
// previous entry is not the read-only segment of the same file.
TEST_F(LinuxCoreDumperTest, EnumerateMappings_kernel_loader_interleaved) {
  const char proc_maps_content[] =
      "3cf2e000-3cfc8000 ---p 00000000 00:00 0 \n"
      "3cfc8000-3d00d000 r--p 00000000 07:d0 14 /apex/bin/linker64\n"
      "3d00d000-3d00f000 rw-p 00000000 00:00 0 [anon:bionic_alloc]\n"
      "3d00f000-3d010000 ---p 00000000 00:00 0 \n"
      "3d010000-3d018000 rw-p 00000000 00:00 0 [anon:signal stack]\n"
      "3d018000-3d116000 r-xp 00050000 07:d0 14 /apex/bin/linker64\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());

  ASSERT_TRUE(dumper.EnumerateMappings());

  EXPECT_EQ(1U, CountModulesByName(dumper, "/apex/bin/linker64"));

  const MappingInfo* module =
      FindModuleByName(dumper, "/apex/bin/linker64");
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(0x3cfc8000U, module->start_addr);
  EXPECT_EQ(0U, module->offset);
  EXPECT_EQ(0x14e000U, EmittedSize(module));
}

// A named, file-backed mapping can land in the hole the kernel left between
// the executable's segments -- the hole sits high in the mmap region, which
// is where the top-down allocator hands out new mappings. Attaching across it
// would emit two overlapping modules, and the processor truncates the lower
// of the two, clipping this module short of the code it is meant to cover.
TEST_F(LinuxCoreDumperTest, EnumerateMappings_named_mapping_in_hole) {
  const char proc_maps_content[] =
      "50c40000-50ca2000 r--p 00000000 fe:07 85 /odm/bin/hw/service\n"
      "50ca2000-50cb0000 r--p 00000000 00:0c 21 /dev/__properties__/u:object_r:x:s0\n"
      "50cb0000-50e21000 r-xp 00070000 fe:07 85 /odm/bin/hw/service\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());
  ASSERT_TRUE(dumper.EnumerateMappings());

  // No two mappings that reach the module list may overlap.
  for (size_t i = 0; i < dumper.mappings().size(); ++i) {
    const MappingInfo* a = dumper.mappings()[i];
    if (!Includable(a))
      continue;
    for (size_t j = i + 1; j < dumper.mappings().size(); ++j) {
      const MappingInfo* b = dumper.mappings()[j];
      if (!Includable(b))
        continue;
      EXPECT_TRUE(a->start_addr + EmittedSize(a) <= b->start_addr ||
                  b->start_addr + EmittedSize(b) <= a->start_addr)
          << "overlap: [" << a->start_addr << ","
          << a->start_addr + EmittedSize(a)
          << ") vs [" << b->start_addr << ","
          << b->start_addr + EmittedSize(b) << ")";
    }
  }
}

// A mapping swallowed by the merge must keep bounding its own stack. Breakpad
// copies up to 32 KB from the stack pointer to the end of the containing
// mapping; if the merged module answers instead, unrelated memory is recorded
// as thread stack.
TEST_F(LinuxCoreDumperTest, GetStackInfo_respects_swallowed_mapping) {
  const char proc_maps_content[] =
      "3cfc8000-3d00d000 r--p 00000000 07:d0 14 /apex/bin/linker64\n"
      "3d010000-3d018000 rw-p 00000000 00:00 0 [anon:thread signal stack]\n"
      "3d018000-3d116000 r-xp 00050000 07:d0 14 /apex/bin/linker64\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());
  ASSERT_TRUE(dumper.EnumerateMappings());

  // A stack pointer one page below the end of the signal stack has exactly
  // one page left, whatever the linker module now spans.
  const uintptr_t sp = 0x3d017000;
  const void* stack = nullptr;
  size_t stack_len = 0;
  ASSERT_TRUE(dumper.GetStackInfo(&stack, &stack_len, sp));
  EXPECT_EQ(0x1000U, stack_len);
}

// lld can emit more than one executable PT_LOAD. Both segments must attach to
// the same header mapping rather than forming a second, unidentifiable module.
TEST_F(LinuxCoreDumperTest, EnumerateMappings_two_exec_segments) {
  const char proc_maps_content[] =
      "50c40000-50ca2000 r--p 00000000 fe:07 85 /odm/bin/hw/service\n"
      "50cb0000-50d00000 r-xp 00070000 fe:07 85 /odm/bin/hw/service\n"
      "50d10000-50e21000 r-xp 000d0000 fe:07 85 /odm/bin/hw/service\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());
  ASSERT_TRUE(dumper.EnumerateMappings());

  EXPECT_EQ(1U, CountModulesByName(dumper, "/odm/bin/hw/service"));

  const MappingInfo* module =
      FindModuleByName(dumper, "/odm/bin/hw/service");
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(0x50c40000U, module->start_addr);
  EXPECT_EQ(0U, module->offset);
  EXPECT_EQ(0x1e1000U, EmittedSize(module));
}

// Same as above, but the second executable segment is adjacent to the first,
// so the merge above swallows it and grows the follower in place. The span is
// recorded on the header only, and the merge never reaches that code, so the
// header must not be left reporting a range that stops short of the code.
TEST_F(LinuxCoreDumperTest, EnumerateMappings_merge_onto_follower) {
  const char proc_maps_content[] =
      "50c40000-50ca2000 r--p 00000000 fe:07 85 /odm/bin/hw/service\n"
      "50cb0000-50d00000 r-xp 00070000 fe:07 85 /odm/bin/hw/service\n"
      "50d00000-50e21000 r-xp 000c0000 fe:07 85 /odm/bin/hw/service\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());
  ASSERT_TRUE(dumper.EnumerateMappings());

  EXPECT_EQ(1U, CountModulesByName(dumper, "/odm/bin/hw/service"));

  const MappingInfo* module =
      FindModuleByName(dumper, "/odm/bin/hw/service");
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(0x50c40000U, module->start_addr);
  EXPECT_EQ(0U, module->offset);
  EXPECT_EQ(0x1e1000U, EmittedSize(module));
}

// lld packs the file, so a segment's p_vaddr and p_offset need not be equal:
// the load bias differs per segment. /vendor/lib64/libfastvd.so in a real
// capture maps r--p at offset 0 from e4a85e632000 and r-xp at offset 0x1e000
// from e4a85e651000, a 0x1000 shift. Attaching must not depend on
// start_addr - offset landing exactly on the header mapping.
TEST_F(LinuxCoreDumperTest, EnumerateMappings_vaddr_padded_exec_segment) {
  const char proc_maps_content[] =
      "50c40000-50ca2000 r--p 00000000 fe:07 85 /odm/bin/hw/service\n"
      "50cb0000-50e21000 r-xp 0006f000 fe:07 85 /odm/bin/hw/service\n"
      "50e30000-50e3d000 r--p 001ee000 fe:07 85 /odm/bin/hw/service\n"
      "50e4c000-50e50000 rw-p 00209000 fe:07 85 /odm/bin/hw/service\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());
  ASSERT_TRUE(dumper.EnumerateMappings());

  EXPECT_EQ(1U, CountModulesByName(dumper, "/odm/bin/hw/service"));

  const MappingInfo* module =
      FindModuleByName(dumper, "/odm/bin/hw/service");
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(0x50c40000U, module->start_addr);
  EXPECT_EQ(0U, module->offset);
  EXPECT_EQ(0x1e1000U, EmittedSize(module));
}

// EnumerateMappings() moves the module containing AT_ENTRY to index 0, because
// the format takes the first module to be the main executable. For a split
// image that entry point lands in a segment which is not the module, and the
// scan must not revisit it. Ordering the split library below the executable
// puts the segment after its own base in the list, which is the shape that
// made an earlier version of this loop spin forever.
TEST_F(LinuxCoreDumperTest, EnumerateMappings_entry_point_with_split_images) {
  const char proc_maps_content[] =
      "40000000-40062000 r--p 00000000 fe:05 977 /vendor/lib64/libfastvd.so\n"
      "40070000-40100000 r-xp 00070000 fe:05 977 /vendor/lib64/libfastvd.so\n"
      "50c40000-50ca2000 r--p 00000000 fe:07 85 /odm/bin/hw/service\n"
      "50cb0000-50e21000 r-xp 00070000 fe:07 85 /odm/bin/hw/service\n";

  std::string test_file = temp_dir.path() + "/maps";
  ASSERT_TRUE(WriteFile(test_file.c_str(), proc_maps_content,
                        sizeof(proc_maps_content)));

  TestableCoreDumper dumper(0, "core_file", temp_dir.path().c_str());
  dumper.SetEntryPoint(0x50cb0000);  // in the executable's code segment

  ASSERT_TRUE(dumper.EnumerateMappings());

  // The mapping holding the ELF header is promoted, not the code segment.
  ASSERT_LE(1U, dumper.mappings().size());
  EXPECT_EQ(0x50c40000U, dumper.mappings()[0]->start_addr);
  EXPECT_EQ(0U, dumper.mappings()[0]->offset);
  EXPECT_STREQ("/odm/bin/hw/service", dumper.mappings()[0]->name);

  // The library below it is still recorded as one module.
  EXPECT_EQ(1U, CountModulesByName(dumper, "/vendor/lib64/libfastvd.so"));
}

}  // namespace google_breakpad

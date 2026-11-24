#include "NativeFileDialog.h"
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <algorithm>
#include <stdexcept>

namespace NativeFileDialog {
namespace {
NSString* Text(const std::string& value)
{
    if (value.find('\0') != std::string::npos) throw std::invalid_argument("File selection options contain a NUL character");
    NSString* text = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
    if (!text) throw std::invalid_argument("File selection options must contain valid UTF-8 text");
    return text;
}

void Filters(NSSavePanel* panel, const Options& options)
{
    if (options.mode == Mode::Folder || options.filters.empty()) return;
    NSMutableArray<UTType*>* types = [NSMutableArray array];
    bool unrestricted = false;
    for (const auto& filter : options.filters) for (const auto& pattern : filter.patterns) {
        if (pattern == "*" || pattern == "*.*") { unrestricted = true; continue; }
        // macOS content types use extensions; a non-extension glob must not
        // prevent selection of otherwise valid source files.
        if (pattern.rfind("*.", 0) != 0 || pattern.size() <= 2 ||
            pattern.find_first_of("*?[]", 2) != std::string::npos) { unrestricted = true; continue; }
        UTType* type = [UTType typeWithFilenameExtension:Text(pattern.substr(2)) conformingToType:UTTypeData];
        if (type && ![types containsObject:type]) [types addObject:type];
        else if (!type) unrestricted = true;
    }
    if (!unrestricted && types.count) {
        panel.allowedContentTypes = types;
        panel.allowsOtherFileTypes = NO;
    } else {
        panel.allowsOtherFileTypes = YES;
    }
}

void InitialPath(NSSavePanel* panel, const Options& options)
{
    if (options.initial.empty()) return;
    std::error_code error;
    if (std::filesystem::is_directory(options.initial, error)) {
        panel.directoryURL = [NSURL fileURLWithPath:Text(std::filesystem::absolute(options.initial).u8string()) isDirectory:YES];
        return;
    }
    if (options.initial.has_parent_path()) {
        const auto parent = std::filesystem::absolute(options.initial.parent_path());
        if (std::filesystem::is_directory(parent, error))
            panel.directoryURL = [NSURL fileURLWithPath:Text(parent.u8string()) isDirectory:YES];
    }
    if (options.mode == Mode::Save) panel.nameFieldStringValue = Text(options.initial.filename().u8string());
}
}

Result ShowMac(const Options& options)
{
    @autoreleasepool {
        if (![NSThread isMainThread]) return {Status::Failed, {}, "File selection must run on the application UI thread."};
        try {
            [NSApplication sharedApplication];
            NSSavePanel* panel;
            NSOpenPanel* open = nil;
            if (options.mode == Mode::Save) panel = [NSSavePanel savePanel];
            else {
                open = [NSOpenPanel openPanel];
                open.canChooseFiles = options.mode == Mode::Open;
                open.canChooseDirectories = options.mode == Mode::Folder;
                open.treatsFilePackagesAsDirectories = options.mode == Mode::Folder;
                open.allowsMultipleSelection = options.multiple && options.mode == Mode::Open;
                panel = open;
            }
            panel.title = Text(options.title);
            panel.canCreateDirectories = YES;
            Filters(panel, options);
            InitialPath(panel, options);
            [NSApp activateIgnoringOtherApps:YES];
            if ([panel runModal] != NSModalResponseOK) return {Status::Cancelled, {}, {}};
            if (!open && !panel.URL) return {Status::Failed, {}, "File selection returned no path."};
            NSArray<NSURL*>* urls = open ? open.URLs : @[panel.URL];
            Result result{Status::Selected, {}, {}};
            for (NSURL* url in urls) {
                const auto* path = url.fileSystemRepresentation;
                if (!url.isFileURL || !path || !*path)
                    return {Status::Failed, {}, "File selection returned an invalid local path."};
                result.paths.push_back(std::filesystem::u8path(path));
            }
            if (result.paths.empty()) return {Status::Failed, {}, "File selection returned no path."};
            return result;
        } catch (const std::exception& error) {
            return {Status::Failed, {}, error.what()};
        }
    }
}
}

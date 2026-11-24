#!/bin/sh
# Use authenticated official HTTPS repositories without modifying runner sources.
# The runner's regional HTTP mirror can take an hour for the Arm C++ libraries.
set -eu
task_codename=$(. /etc/os-release; printf '%s' "$VERSION_CODENAME")
task_arch=$(dpkg --print-architecture)
task_directory=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/lvgl-ci-apt.XXXXXX")
task_sources="$task_directory/ubuntu.sources"
case "$task_arch" in
    amd64) task_archive=https://archive.ubuntu.com/ubuntu; task_security=https://security.ubuntu.com/ubuntu ;;
    arm64) task_archive=https://ports.ubuntu.com/ubuntu-ports; task_security=$task_archive ;;
    *) printf 'Unsupported Ubuntu CI architecture: %s\n' "$task_arch" >&2; exit 1 ;;
esac
cat > "$task_sources" <<EOF
Types: deb
URIs: $task_archive
Suites: $task_codename $task_codename-updates
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: $task_security
Suites: $task_codename-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
apt_ci() {
    sudo env DEBIAN_FRONTEND=noninteractive NEEDRESTART_MODE=a apt-get \
        -o "Dir::Etc::sourcelist=$task_sources" -o 'Dir::Etc::sourceparts=-' \
        -o 'APT::Get::List-Cleanup=0' -o 'Acquire::Retries=2' \
        -o 'Acquire::http::Timeout=30' -o 'Acquire::https::Timeout=30' \
        -o 'Dpkg::Use-Pty=0' "$@"
}
apt_ci update
apt_ci install -y --no-install-recommends "$@"

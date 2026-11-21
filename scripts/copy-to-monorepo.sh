#!/usr/bin/env bash
#
# copy-to-monorepo.sh - copy the virtio-snd driver sources into a ReactOS
# monorepo checkout
#
# Usage:
#   scripts/copy-to-monorepo.sh [<reactos-checkout>]
#
#   <reactos-checkout>  Path to the target ReactOS source tree.
#                       Default: ../reactos-feature  (relative to this repo).
#
# Destination: <reactos-checkout>/drivers/wdm/audio/drivers/virtiosnd/
#
set -euo pipefail

# Repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# default to reactos-feature (as we keep the "reactos" checkout on master as a pristine build collaborator)
TARGET_REPO="${1:-${SRC_ROOT}/../reactos-feature}"

if [ ! -d "${TARGET_REPO}" ]; then
    echo "error: target ReactOS checkout not found: ${TARGET_REPO}" >&2
    echo "       pass the path as the first argument, e.g.:" >&2
    echo "         scripts/copy-to-monorepo.sh /path/to/reactos" >&2
    exit 1
fi
# Does this look like a reactos checkout?
DRIVERS_PARENT="${TARGET_REPO}/drivers/wdm/audio/drivers"
if [ ! -d "${DRIVERS_PARENT}" ]; then
    echo "error: '${TARGET_REPO}' does not look like a ReactOS source tree" >&2
    echo "       (missing drivers/wdm/audio/drivers)." >&2
    exit 1
fi

DEST="${DRIVERS_PARENT}/virtiosnd"

# Check if the source dir looks correct
C_SRCS=("${SRC_ROOT}"/src/*.c)
CPP_SRCS=("${SRC_ROOT}"/src/*.cpp)
HDRS=("${SRC_ROOT}"/include/*.h)
INF="${SRC_ROOT}/virtiosnd.inf"

for f in "${C_SRCS[@]}" "${CPP_SRCS[@]}" "${HDRS[@]}" "${INF}"; do
    [ -e "${f}" ] || { echo "error: expected input missing: ${f}" >&2; exit 1; }
done

# Create target dir from scatch. This overwrites the original
echo ">> target: ${DEST}"
rm -rf "${DEST}"
mkdir -p "${DEST}"

copy() { echo "   + $(basename "$1")"; cp -f "$1" "${DEST}/"; }

echo ">> copying sources"
for f in "${C_SRCS[@]}"; do copy "$f"; done
for f in "${CPP_SRCS[@]}"; do copy "$f"; done
for f in "${HDRS[@]}"; do copy "$f"; done
echo ">> copying INF"
copy "${INF}"

# Let's make the CMakeLists.txt
# Build the SOURCE list dynamically from the .c/.cpp files actually copied into
# the destination, sorted alphabetically (satisfies the reviewer's ordering nit
# and keeps the list in sync if sources are added/removed). Headers are not
# listed, matching ReactOS convention (e.g. ac97).
echo ">> generating CMakeLists.txt"
SOURCE_LINES="$(
    find "${DEST}" -maxdepth 1 -type f \( -name '*.c' -o -name '*.cpp' \) \
        -printf '%f\n' | LC_ALL=C sort | sed 's/^/    /'
)"
if [ -z "${SOURCE_LINES}" ]; then
    echo "error: no .c/.cpp sources found in ${DEST}" >&2
    exit 1
fi

cat > "${DEST}/CMakeLists.txt" <<CMAKE

option(VIRTIO_SND_TEST_XRUN_INJECTION
    "Inject one synthetic XRUN per playback stream for recovery testing"
    OFF)

list(APPEND SOURCE
${SOURCE_LINES})

add_library(virtiosnd MODULE \${SOURCE})

target_link_libraries(virtiosnd virtio stdunk libcntpr uuid)

if(VIRTIO_SND_TEST_XRUN_INJECTION)
    target_compile_definitions(virtiosnd PRIVATE VIRTIO_SND_TEST_XRUN_INJECTION=1)
    message(STATUS "Building virtiosnd with synthetic XRUN injection enabled")
endif()

set_module_type(virtiosnd wdmdriver)
add_importlibs(virtiosnd portcls hal ntoskrnl)
add_cd_file(TARGET virtiosnd DESTINATION reactos/system32/drivers FOR all)
add_driver_inf(virtiosnd virtiosnd.inf)
CMAKE

echo "   + CMakeLists.txt"

echo ">> done. ${DEST} now contains:"
ls -1 "${DEST}" | sed 's/^/     /'

# drivers/wdm/audio/drivers/CMakeLists.txt was edited manually, no need to have it in the script

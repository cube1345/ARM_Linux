#!/usr/bin/env bash
set -Ee
set -o pipefail

PROJECT_DIR="/home/cube/WorkSpace/ARM_Linux/apps/PhotoAlbum/qt"
TOOLCHAIN="${PROJECT_DIR}/cmake/toolchain-arm.cmake"
BUILD_DIR="/tmp/photoalbum-arm-build"
OUTPUT_FILE="${BUILD_DIR}/photo-album"
DIST_DIR="${PROJECT_DIR}/.dist"
DIST_FILE="${DIST_DIR}/photo-album"

if [[ ! -f "${TOOLCHAIN}" ]]; then
    echo "错误：找不到 CMake toolchain 文件：${TOOLCHAIN}" >&2
    exit 1
fi

command -v cmake >/dev/null || {
    echo "错误：cmake 不可用，请检查 PATH。" >&2
    exit 1
}

echo "开始编译 ARM Qt 程序（CMake）..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" "${DIST_DIR}"

cd "${BUILD_DIR}"
cmake "${PROJECT_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DPHOTOALBUM_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"

file "${OUTPUT_FILE}"

if ! file "${OUTPUT_FILE}" | grep -q "ARM"; then
    echo "错误：生成文件不是 ARM 程序。" >&2
    exit 1
fi

cp -f "${OUTPUT_FILE}" "${DIST_FILE}"

echo
echo "编译完成："
echo "构建文件：${OUTPUT_FILE}"
echo "发布文件：${DIST_FILE}"
echo
md5sum "${DIST_FILE}"

#!/bin/sh
# Build the one pinned libav feature set Moon Engine links into its binary:
# MPEG-PS/IPU demuxers, MPEG-2/IPU decoders and the filters the movie path uses.
# No programs, no network, no GPL, static archives only. The raw `mpegvideo`
# demuxer is the codec probe MPEG-PS needs when a stream has no program stream
# map; the `ipu` parser splits the IPU archive's raw pictures; `il` deinterlaces
# the interlaced MPEG-2 menu movies.
#
#   tools/build_libav.sh [linux] [windows] [android-arm64]
#
# Defaults to every target. Outputs build/libav/<target>/lib/libav*.a and is
# idempotent: a target whose configuration did not change is left alone.
# FFMPEG_SRC names an existing n7.1 tree; otherwise the script clones it.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TAG=n7.1
SRC=${FFMPEG_SRC:-$ROOT/build/libav/ffmpeg-src}
OUT=$ROOT/build/libav
JOBS=${JOBS:-4}
ANDROID_NDK=${ANDROID_NDK:-${ANDROID_HOME:-/mnt/ssd/android-sdk}/ndk/27.2.12479018}

CONFIG="--disable-everything --disable-programs --disable-doc --disable-network \
--disable-autodetect --disable-debug --disable-shared --enable-static --disable-gpl \
--enable-demuxer=mpegps --enable-demuxer=ipu --enable-demuxer=mpegvideo \
--enable-decoder=mpeg2video --enable-decoder=ipu \
--enable-parser=mpegvideo --enable-parser=ipu \
--enable-protocol=file \
--enable-filter=scale --enable-filter=format --enable-filter=null --enable-filter=fps \
--enable-filter=setfield --enable-filter=yadif --enable-filter=il \
--enable-swscale --enable-avfilter --enable-avformat --enable-avcodec --enable-avutil"

need_source() {
    if [ -f "$SRC/configure" ] && [ "$(cat "$SRC/RELEASE" 2>/dev/null)" = 7.1 ]; then
        return
    fi
    if [ -e "$SRC" ] && [ ! -f "$SRC/configure" ]; then
        echo "libav: $SRC is not FFmpeg $TAG; remove it or set FFMPEG_SRC" >&2
        exit 1
    fi
    if [ ! -e "$SRC" ]; then
        echo "libav: cloning FFmpeg $TAG into $SRC"
        mkdir -p "$(dirname "$SRC")"
        git clone --depth 1 --branch "$TAG" https://github.com/FFmpeg/FFmpeg.git "$SRC"
    fi
    if [ "$(cat "$SRC/RELEASE" 2>/dev/null)" != 7.1 ]; then
        echo "libav: $SRC is not FFmpeg 7.1 (RELEASE=$(cat "$SRC/RELEASE" 2>/dev/null))" >&2
        exit 1
    fi
}

build_target() {
    name=$1
    shift
    dir=$OUT/$name
    build=$dir/build
    lib=$dir/lib
    stamp=$dir/config.stamp
    config="$CONFIG $*"

    if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$config" ] &&
        [ -f "$lib/libavcodec.a" ] && [ -f "$lib/libavformat.a" ] &&
        [ -f "$lib/libavutil.a" ] && [ -f "$lib/libswscale.a" ] && [ -f "$lib/libavfilter.a" ]; then
        echo "libav: $name already built"
    else
        echo "libav: configuring $name"
        rm -rf "$build" "$lib"
        mkdir -p "$build" "$lib"
        # shellcheck disable=SC2086
        ( cd "$build" && "$SRC/configure" --prefix="$dir" $config ) \
            > "$dir/configure.log" 2>&1 || {
            echo "libav: configure failed for $name (see $dir/configure.log)" >&2
            return 1
        }
        echo "libav: building $name"
        ( cd "$build" && make -j"$JOBS" ) > "$dir/build.log" 2>&1 || {
            echo "libav: build failed for $name (see $dir/build.log)" >&2
            return 1
        }
        cp "$build"/libavcodec/libavcodec.a "$build"/libavformat/libavformat.a \
           "$build"/libavutil/libavutil.a "$build"/libswscale/libswscale.a \
           "$build"/libavfilter/libavfilter.a "$lib"/
        printf '%s\n' "$config" > "$stamp"
    fi
    for archive in avcodec avformat avutil swscale avfilter; do
        f=$lib/lib$archive.a
        printf 'libav: %-14s %10s bytes  %s\n' "$name" "$(wc -c < "$f")" "$f"
    done
}

need_source

targets=${*:-linux windows android-arm64}
failed=
for target in $targets; do
    case $target in
    linux)
        build_target linux || failed="$failed $target"
        ;;
    windows)
        if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
            echo "libav: windows skipped (x86_64-w64-mingw32-gcc not found)" >&2
            failed="$failed $target"
            continue
        fi
        build_target windows --arch=x86_64 --target-os=mingw32 --enable-cross-compile \
            --cross-prefix=x86_64-w64-mingw32- || failed="$failed $target"
        ;;
    android-arm64)
        toolchain=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64
        if [ ! -x "$toolchain/bin/aarch64-linux-android26-clang" ]; then
            echo "libav: android-arm64 skipped (NDK 27.2.12479018 not found at $ANDROID_NDK)" >&2
            failed="$failed $target"
            continue
        fi
        build_target android-arm64 --arch=aarch64 --target-os=android --enable-cross-compile \
            --enable-pic \
            --cc="$toolchain/bin/aarch64-linux-android26-clang" \
            --cxx="$toolchain/bin/aarch64-linux-android26-clang++" \
            --ar="$toolchain/bin/llvm-ar" --ranlib="$toolchain/bin/llvm-ranlib" \
            --nm="$toolchain/bin/llvm-nm" --strip="$toolchain/bin/llvm-strip" \
            --sysroot="$toolchain/sysroot" || failed="$failed $target"
        ;;
    *)
        echo "libav: unknown target $target" >&2
        failed="$failed $target"
        ;;
    esac
done

if [ -n "$failed" ]; then
    echo "libav: failed targets:$failed" >&2
    exit 1
fi
echo "libav: all requested targets built"

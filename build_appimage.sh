#!/bin/bash
set -e

echo "=== 1. Подготовка инструментов ==="
mkdir -p .tools
cd .tools

[ -f linuxdeploy-x86_64.AppImage ] || wget -q --show-progress https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
[ -f linuxdeploy-plugin-qt-x86_64.AppImage ] || wget -q --show-progress https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
[ -f runtime-x86_64 ] || wget -q --show-progress https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64
[ -f appimagetool-x86_64.AppImage ] || wget -q --show-progress https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x linuxdeploy*.AppImage appimagetool-x86_64.AppImage

echo "=== 2. Распаковка утилит ==="
rm -rf squashfs-root linuxdeploy appimagetool

./linuxdeploy-x86_64.AppImage --appimage-extract > /dev/null
mv squashfs-root linuxdeploy

./linuxdeploy-plugin-qt-x86_64.AppImage --appimage-extract > /dev/null
cp squashfs-root/AppRun linuxdeploy/linuxdeploy-plugin-qt
rm -rf squashfs-root

# Распаковываем appimagetool (чтобы не зависеть от FUSE)
./appimagetool-x86_64.AppImage --appimage-extract > /dev/null
mv squashfs-root appimagetool

cd ..

echo "=== 3. Сборка проекта ==="
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

echo "=== 4. Подготовка AppDir ==="
rm -rf AppDir
mkdir -p AppDir/usr/bin
cp build/hofpad AppDir/usr/bin/
cp resources/hofpadwhite.png AppDir/

echo "=== 5. Упаковка библиотек (linuxdeploy) ==="
export QMAKE=$(which qmake6 || which qmake || echo "")
export PATH="$PWD/.tools/linuxdeploy:$PATH"

.tools/linuxdeploy/AppRun --appdir=AppDir \
    --plugin=qt \
    --desktop-file=hofpad.desktop \
    --icon-file=AppDir/hofpadwhite.png

echo "=== 6. Создание AppImage (appimagetool) ==="
# Запускаем appimagetool, передаем ему наш runtime
export ARCH=x86_64
.tools/appimagetool/AppRun AppDir Hofpad-x86_64.AppImage --runtime-file .tools/runtime-x86_64

echo "=== ГОТОВО! ==="
ls -la *.AppImage

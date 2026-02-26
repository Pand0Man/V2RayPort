# V2RayPort (Windows, C++)

`V2RayPort` — desktop-клиент под Windows в стиле V2RayTun, но для ПК.

## Что реализовано

- C++ WinAPI-приложение;
- выбор `xray.exe` / `config.json` / `wintun.dll`;
- запуск/остановка `xray.exe`;
- live-логи процесса;
- сохранение последних путей (`v2rayport.ini`).

---

## 0) Установка **Desktop development with C++** (подробно)

Если билд не работает, сначала проверь именно это.

1. Открой **Visual Studio Installer**.
2. Найди **Visual Studio 2022 Build Tools** (или Visual Studio 2022) и нажми **Modify**.
3. Во вкладке **Workloads** поставь галочку:
   - ✅ **Desktop development with C++**
4. Во вкладке **Individual components** проверь, что выбраны:
   - ✅ MSVC v143 (x64/x86 build tools)
   - ✅ Windows 10/11 SDK
   - ✅ C++ CMake tools for Windows (желательно)
5. Нажми **Modify/Install** и дождись окончания.
6. Перезапусти терминал/VS Code.

Проверка после установки:

```powershell
cl
cmake --version
```

Если `cl` не найден, toolchain всё ещё не установлен корректно.

---

## 1) Один НОРМАЛЬНЫЙ способ сборки (рекомендуется)

Запускать **из корня репозитория** (`там где CMakeLists.txt`):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "& '.\\scripts\\build.ps1' -BuildDir '.build' -Config Release -Clean"
```

Что это даёт:
- не создаёт «мусор» по разным папкам (всё в `.build`);
- перед сборкой чистит старую сборку (`-Clean`);
- автоматически выбирает генератор (Visual Studio или Ninja);
- падает с ошибкой, если конфиг/билд реально не прошли.

Готовый exe:
- если Visual Studio generator: `.build\Release\V2RayPort.exe`
- если Ninja generator: `.build\V2RayPort.exe`

---

## 2) Почему у тебя «нет папки Release»

Это нормально, если выбран **Ninja** (single-config генератор).

- Visual Studio generator → есть `Release` подпапка.
- Ninja generator → `Release` подпапки нет, exe лежит прямо в `.build\`.

---

## 3) Runtime (xray + wintun) без мусора

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -Command "& '.\\scripts\\fetch-runtime.ps1' -OutputDir '.build'"
```

Будет:
- `.build\xray.exe`
- `.build\wintun.dll`

Временные архивы/распаковки удаляются автоматически.

---

## 4) Если команда не запускается вообще

### Ошибка про ExecutionPolicy

Запусти так (в текущем окне):

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build.ps1 -BuildDir .build -Config Release -Clean
```

### Ошибка `Generator Visual Studio 17 2022 could not find any instance of Visual Studio`

Значит не установлен Build Tools 2022 с workload C++.
Смотри раздел **0** и доустанови workload.

### Ошибка `cmake` не найден

```powershell
winget install Kitware.CMake
```

Перезапусти PowerShell.

### Ошибка `#include <windows.h>`

Не установлен Windows SDK.
Установи компонент **Windows 10/11 SDK** через Visual Studio Installer.

---

## 5) Запуск

1. Запусти `V2RayPort.exe` из `.build` (или `.build\Release`).
2. Укажи пути к `xray.exe`, `config.json`, `wintun.dll`.
3. Нажми «Запустить».

---

## Ограничения MVP

- Это пока desktop-оболочка вокруг `xray.exe`;
- нет профилей/подписок;
- нет системного прокси/трея/автозапуска;
- нет полного TUN-менеджмента как в мобильных клиентах.

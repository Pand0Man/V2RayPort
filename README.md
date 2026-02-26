# V2RayPort (Windows, C++)

`V2RayPort` — desktop-клиент под Windows в стиле V2RayTun, но для ПК.

## Что реализовано

- C++ WinAPI-приложение с улучшенным интерфейсом (градиентный фон, аккуратная типографика, визуальные блоки);
- анимации статуса: пульсирующий индикатор и progress marquee во время запуска;
- выбор `xray.exe`;
- выбор `config.json`;
- опциональный выбор `wintun.dll` (добавляется в `PATH` перед запуском);
- запуск/остановка `xray.exe`;
- онлайн-логи процесса в окне приложения;
- сохранение последних путей (`v2rayport.ini`) для быстрого повторного запуска;
- быстрые кнопки «Открыть папку runtime» и «Открыть папку config».

---

## Подготовка runtime-файлов (оригинальные xray-core + wintun)

В проект добавлен скрипт `scripts/fetch-runtime.ps1`, который:
- скачивает **последний релиз Xray-core** с официального GitHub `XTLS/Xray-core`;
- извлекает из релиза `xray.exe`;
- скачивает официальный архив `wintun`;
- извлекает `wintun.dll`;
- кладёт оба файла в папку `build\Release/` (туда же, где билдится `V2RayPort.exe`);
- после копирования автоматически удаляет zip-архивы и временные папки распаковки.

Запуск (PowerShell):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\fetch-runtime.ps1
```

После этого получите:
- `build\Release\xray.exe`
- `build\Release\wintun.dll`

Дополнительно:

```powershell
# указать папку/архитектуру явно
powershell -ExecutionPolicy Bypass -File .\scripts\fetch-runtime.ps1 -OutputDir build\Release -Arch 64

# оставить временные файлы (архивы и распаковку)
powershell -ExecutionPolicy Bypass -File .\scripts\fetch-runtime.ps1 -KeepTemp
```

---

## Супер-подробный гайд по сборке в VS Code (Windows)

Ниже — максимально практичный путь «с нуля», если не билдится.

### Шаг 0. Что должно быть установлено

Обязательно:
1. **Visual Studio 2022 Build Tools** или полная **Visual Studio 2022**.
2. Workload: **Desktop development with C++**.
3. Компонент: **Windows 10/11 SDK**.
4. **CMake** (можно отдельным инсталлером или из Visual Studio).
5. **VS Code**.

Быстрые команды установки (по желанию):

```powershell
winget install Kitware.CMake
winget install Microsoft.VisualStudio.2022.BuildTools
winget install Microsoft.VisualStudioCode
```

> После установки Build Tools проверь в Visual Studio Installer, что реально отмечены:
> - MSVC v143
> - Windows 10/11 SDK
> - C++ CMake tools for Windows (желательно)

### Шаг 1. Установка расширений в VS Code

Открой VS Code → Extensions (`Ctrl+Shift+X`) и установи:
- **C/C++** (ms-vscode.cpptools)
- **CMake Tools** (ms-vscode.cmake-tools)

### Шаг 2. Открытие проекта

1. `File -> Open Folder...`
2. Выбери папку проекта `V2RayPort`.
3. Дождись индексации расширений.

### Шаг 3. Запуск правильного терминала в VS Code

Очень важно: обычный PowerShell может не видеть SDK/компилятор.

Рекомендуется:
1. `Terminal -> New Terminal`
2. Нажать стрелку рядом с `+` в терминале и выбрать:
   - **Developer PowerShell for VS 2022**
   - или **x64 Native Tools Command Prompt for VS 2022**

Проверка в терминале:

```powershell
cl
cmake --version
```

Если `cl` не найден — ты не в developer-shell.

### Шаг 4. Начиная с `CMake: Select a Kit` — максимально просто

Открой Command Palette (`Ctrl+Shift+P`) и делай **ровно в таком порядке**:

1. `CMake: Scan for Kits`
2. `CMake: Select a Kit`
   - выбери пункт вроде: **Visual Studio Community 2022 Release - amd64**
   - главное, чтобы было **2022** и **x64/amd64**
3. `CMake: Select Build Variant` → выбери **Release**
4. `CMake: Configure`
5. `CMake: Build`

Если всё ок, внизу VS Code будет `Build finished successfully`, а файл появится здесь:
- `build\Release\V2RayPort.exe`

### Шаг 5. Если в UI не получается — одна команда в PowerShell

В терминале VS Code (лучше Developer PowerShell) выполни **одну команду**:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDir build -Config Release -Generator "Visual Studio 17 2022" -Arch x64
```

Это самый надёжный вариант, потому что скрипт сам:
- ищет `cmake` в `PATH`;
- если не находит, ищет встроенный `cmake.exe` внутри Visual Studio;
- предупреждает, если не найден `cl.exe`;
- выполняет configure + build.

### Шаг 6. Альтернатива: тоже одной командой, но без скрипта

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64; if ($?) { cmake --build build --config Release }
```

### Шаг 7. Подтянуть runtime рядом с exe

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\fetch-runtime.ps1
```

После этого рядом с exe будут:
- `build\Release\xray.exe`
- `build\Release\wintun.dll`

### Шаг 8. Запуск

1. Запусти `build\Release\V2RayPort.exe`.
2. Укажи пути к `xray.exe`, `config.json`, `wintun.dll`.
3. Нажми **«Запустить»**.

---

## Быстрый CLI-вариант сборки (без UI VS Code)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Или в одну строку:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64; if ($?) { cmake --build build --config Release }
```

---

## Решение частых ошибок в VS Code

### Ошибка: `cmake : Имя "cmake" не распознано...`

Причина: CMake не установлен или не в PATH.

Решение:
1. Запусти `scripts/build.ps1`.
2. Если не помогло — установи CMake:

```powershell
winget install Kitware.CMake
```

3. Полностью перезапусти VS Code.

### Ошибка: `#include <windows.h>` (как на твоём скрине)

Причина:
- не установлен Windows SDK,
- не установлен workload C++,
- открыт не developer-shell.

Решение:
1. В Visual Studio Installer включи **Desktop development with C++**.
2. Проверь, что установлен **Windows 10/11 SDK**.
3. В VS Code используй **Developer PowerShell for VS 2022**.
4. Проверь:

```powershell
cl
where cl
```

5. Повтори сборку через:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

### CMake Tools пишет, что не найден kit

1. `Ctrl+Shift+P` → `CMake: Scan for Kits`
2. Затем `CMake: Select a Kit` → Visual Studio 2022 x64
3. `CMake: Delete Cache and Reconfigure`

### CMake Cache сломан после неудачных попыток

```powershell
Remove-Item -Recurse -Force .\build
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

---

## Ограничения текущего MVP

- Пока это desktop-оболочка вокруг `xray.exe`, а не полный клон мобильного V2RayTun;
- нет профилей/подписок;
- нет системного прокси/трея/автозапуска;
- нет полного TUN-менеджмента как в мобильных клиентах.

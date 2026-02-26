# V2RayPort (Windows, C++)

`V2RayPort` — desktop-клиент под Windows в стиле V2RayTun, но для ПК.

## Что реализовано

- C++ WinAPI-приложение;
- выбор `xray.exe` / `config.json` / `wintun.dll`;
- запуск/остановка `xray.exe`;
- live-логи процесса;
- сохранение последних путей (`v2rayport.ini`).

---

## ВАЖНО: только командная сборка (без UI-шагов VS Code)

Ниже только рабочие команды. Никаких `Select a Kit` и других UI-этапов.

---

## 1) Минимальные требования

На Windows должны быть:
- CMake
- Компилятор C++ для Windows (любой один вариант):
  - Visual Studio 2022 Build Tools + workload `Desktop development with C++` (рекомендуется)
  - или Ninja + clang++/g++ (альтернатива)

Проверка:

```powershell
cmake --version
```

Если Visual Studio toolchain установлен, дополнительно:

```powershell
cl
```

---

## 2) ОДНА команда на билд (рекомендуется)

Запусти из корня проекта:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDir build -Config Release
```

Что делает скрипт:
- автоматически выбирает генератор:
  - `Visual Studio 17 2022`, если VS найден;
  - иначе `Ninja`, если он есть;
- корректно завершает сборку с ошибкой, если configure/build упал;
- показывает фактический путь к `V2RayPort.exe`.

---

## 3) Если хочешь без скрипта — команды руками

### Вариант A (есть Visual Studio Build Tools)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

EXE будет в:
- `build\Release\V2RayPort.exe`

### Вариант B (нет Visual Studio, но есть Ninja + компилятор)

```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

EXE будет в:
- `build\V2RayPort.exe`

> Поэтому у тебя могло «не быть папки Release» — это нормально для `Ninja`.

---

## 4) Скачивание runtime (xray + wintun)

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\fetch-runtime.ps1 -OutputDir build
```

После этого в `build\` будут:
- `xray.exe`
- `wintun.dll`

Если билдил через Visual Studio и хочешь именно в `build\Release`:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\fetch-runtime.ps1 -OutputDir build\Release
```

Скрипт чистит временные архивы автоматически.

---

## 5) Частые ошибки и точные решения

### Ошибка: `Generator Visual Studio 17 2022 could not find any instance of Visual Studio`

Причина: Visual Studio Build Tools не установлен (или установлен без C++ workload).

Решение:
1. Установи Build Tools 2022 и workload `Desktop development with C++`.
2. Либо используй Ninja-вариант (если есть `ninja` + компилятор).
3. Для авто-выбора генератора запускай `scripts/build.ps1`.

### Ошибка: `cl.exe не найден`

Причина: нет MSVC toolchain в текущей системе/терминале.

Решение:
- либо установить Build Tools,
- либо собирать через Ninja с установленным clang++/g++.

### Ошибка: `cmake не распознано`

```powershell
winget install Kitware.CMake
```

Перезапусти PowerShell после установки.

### Ошибка: `#include <windows.h>`

Нет Windows SDK.
Установи в Visual Studio Installer компонент **Windows 10/11 SDK**.

---

## 6) Запуск

1. Запусти exe (см. путь в зависимости от генератора).
2. Укажи пути к `xray.exe`, `config.json`, `wintun.dll`.
3. Нажми «Запустить».

---

## Ограничения MVP

- Это пока desktop-оболочка вокруг `xray.exe`;
- нет профилей/подписок;
- нет системного прокси/трея/автозапуска;
- нет полного TUN-менеджмента как в мобильных клиентах.

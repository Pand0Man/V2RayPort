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

## Подробный гайд по сборке V2RayPort

### 1) Требования

- Windows 10/11
- CMake 3.20+
- Visual Studio 2022 (или Build Tools с MSVC v143)

Проверь:

```powershell
cmake --version
cl
```


### Быстрый билд через скрипт (рекомендуется)

Если в PowerShell ошибка вида **`cmake : Имя "cmake" не распознано...`**, используй:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

Скрипт:
- пытается найти `cmake` в `PATH`;
- если не находит — пытается использовать `cmake.exe`, встроенный в Visual Studio;
- если не найдено ничего — печатает точные шаги установки.

### 2) Конфигурация проекта

Из корня репозитория:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

### 3) Сборка

```powershell
cmake --build build --config Release
```

### 4) Результат

Готовый EXE:
- `build\Release\V2RayPort.exe`

---

## Запуск приложения

1. Запусти `build\Release\V2RayPort.exe`.
2. В поле **xray.exe** укажи `build\Release\xray.exe` (или свой путь).
3. В поле **config.json** укажи рабочий конфиг Xray.
4. В поле **wintun.dll** укажи `build\Release\wintun.dll` (если используешь TUN/драйверные сценарии).
5. Нажми **«Запустить»**.

---


## Решение ошибки "cmake не распознано"

Если видишь ошибку как на скриншоте:

```powershell
cmake : Имя "cmake" не распознано как имя командлета...
```

Сделай так:

1. Запусти авто-скрипт сборки:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

2. Если скрипт тоже не нашёл CMake, установи его:

```powershell
winget install Kitware.CMake
```

3. Перезапусти PowerShell и повтори сборку.

## Ограничения текущего MVP

- Пока это desktop-оболочка вокруг `xray.exe`, а не полный клон мобильного V2RayTun;
- нет профилей/подписок;
- нет системного прокси/трея/автозапуска;
- нет полного TUN-менеджмента как в мобильных клиентах.

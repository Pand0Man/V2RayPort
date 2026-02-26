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
- кладёт оба файла в папку `runtime/`.

Запуск (PowerShell):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\fetch-runtime.ps1
```

После этого получите:
- `runtime\xray.exe`
- `runtime\wintun.dll`

> Если хочешь другой OutputDir или архитектуру, используй параметры:
>
> ```powershell
> powershell -ExecutionPolicy Bypass -File .\scripts\fetch-runtime.ps1 -OutputDir runtime -Arch 64
> ```

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
2. В поле **xray.exe** укажи `runtime\xray.exe` (или свой путь).
3. В поле **config.json** укажи рабочий конфиг Xray.
4. В поле **wintun.dll** укажи `runtime\wintun.dll` (если используешь TUN/драйверные сценарии).
5. Нажми **«Запустить»**.

---

## Ограничения текущего MVP

- Пока это desktop-оболочка вокруг `xray.exe`, а не полный клон мобильного V2RayTun;
- нет профилей/подписок;
- нет системного прокси/трея/автозапуска;
- нет полного TUN-менеджмента как в мобильных клиентах.

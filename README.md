# V2RayPort (Windows, C++)

`V2RayPort` — desktop-клиент под Windows в стиле V2RayTun, но для ПК.

## Что реализовано

- C++ WinAPI-приложение с аккуратным интерфейсом (Segoe UI, отдельные блоки настроек и логов);
- выбор `xray.exe`;
- выбор `config.json`;
- опциональный выбор `wintun.dll` (добавляется в `PATH` перед запуском);
- запуск/остановка `xray.exe`;
- онлайн-логи процесса в окне приложения.

## Сборка (Windows)

Требования:
- CMake 3.20+
- Visual Studio Build Tools / MSVC

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Готовый exe:
- `build/Release/V2RayPort.exe`

## Запуск

1. Запустите `V2RayPort.exe`.
2. Укажите путь к `xray.exe`.
3. Укажите путь к `config.json`.
4. При необходимости укажите путь к `wintun.dll`.
5. Нажмите **«Запустить»**.

## Ограничения текущего MVP

- Пока это desktop-оболочка вокруг `xray.exe`, а не полный клон мобильного V2RayTun;
- нет профилей/подписок;
- нет системного прокси/трея/автозапуска;
- нет полного TUN-менеджмента как в мобильных клиентах.

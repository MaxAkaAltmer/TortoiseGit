# TortoiseGit: сборка и запуск под Linux

Инструкция для текущей стадии портирования (Phase 3 — GUI на Qt: главное окно
и диалоги Log/Status/Diff поверх git-слоя `gitcore` на libgit2 и
инфраструктурного слоя Phase 1: CMake + Qt6, утилиты `CrossPlatform/`).
Актуальный статус — в конце документа.

## Требования

| Компонент | Минимум | Примечание |
|---|---|---|
| GCC / Clang | C++20 (GCC ≥ 10 / Clang ≥ 12) | |
| CMake | 3.16 | |
| Qt6 (Core, Gui, Widgets, Network, Concurrent) | 6.2+ | Qt5 допускается как fallback, но не тестировался |

Системный `libgit2` опционален: при его отсутствии собирается встроенный
подмодуль `ext/libgit2` (рекомендуется — версия проверена для этого проекта).
Для тестов нужен `ext/googletest`, для JSON-вывода `tgit-cli` — заголовочный
`ext/json` (nlohmann). Подмодули инициализируются командой из раздела
«Сборка»; `libgit2-dev` из пакетов дистрибутива ставить не обязательно.

### Ubuntu / Debian / Mint

```bash
sudo apt install \
    build-essential cmake git \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools
```

### Fedora / RHEL

```bash
sudo dnf install \
    gcc-c++ cmake git \
    qt6-qtbase-devel qt6-qttools-devel
```

### Arch Linux

```bash
sudo pacman -S \
    base-devel cmake git \
    qt6-base qt6-tools
```

## Сборка

```bash
git clone https://gitlab.com/tortoisegit/tortoisegit.git
cd tortoisegit
git submodule update --init

# как минимум для сборки git-слоя и тестов:
git submodule update --init ext/libgit2 ext/googletest ext/json

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# модульные тесты (25 gitcore + 4 ConfigStore)
cd build && ctest
```

Результат: `build/bin/tortoisegit` (GUI), `build/bin/tgit-cli`
(консольный доступ к git), `build/bin/gitcore_tests` и
`build/bin/configstore_tests` (тесты).

Отключить сборку тестов: `cmake -S . -B build -DTGIT_BUILD_TESTS=OFF`.

Тип сборки по умолчанию — `Debug`; доступные значения `CMAKE_BUILD_TYPE`:
`Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`.

> classes с `Q_OBJECT` обрабатываются автоматически: в `src/CMakeLists.txt`
> включён `CMAKE_AUTOMOC`. При ошибке линковки вида
> `undefined reference to 'vtable for ...'` убедитесь, что заголовок класса
> перечислен в `TGIT_HEADERS`/`TGIT_SOURCES` цели `tortoisegit`.

## Запуск

```bash
./build/bin/tortoisegit --help      # список опций
./build/bin/tortoisegit --version   # версия
```

Без графического сессии (headless/CI) используйте offscreen-платформу:

```bash
QT_QPA_PLATFORM=offscreen ./build/bin/tortoisegit --version
```

### Опции командной строки

| Опция | Описание |
|---|---|
| `-h, --help` | справка по опциям |
| `-v, --version` | версия |
| `-s, --action <action>` | режим диалога: `log`, `status` или `diff` (см. «GUI») |
| `-p, --path <path>` | целевой путь |
| `-u, --url <url>` | URL для clone/push/pull |
| `-r, --revision <revision>` | ревизия/коммит |
| `-d, --debug` | режим отладки |
| `--verbose` | подробный вывод (короткий `-v` занят опцией `--version`) |

## tgit-cli — консольная утилита (Phase 2)

Работает без X11/Wayland. Каталог репозитория — текущий или заданный `-C`.

```bash
./build/bin/tgit-cli init ~/src/demo            # git init
./build/bin/tgit-cli clone <path|url> ~/src/c   # локальные пути уже поддерживаются
./build/bin/tgit-cli -C ~/src/demo status
./build/bin/tgit-cli -C ~/src/demo log -n 10 --json
./build/bin/tgit-cli -C ~/src/demo show HEAD~1
./build/bin/tgit-cli -C ~/src/demo diff HEAD -- src/main.cpp
./build/bin/tgit-cli trust /mnt/other-user/repo # добавляет в safe.directory
```

| Команда | Назначение |
|---|---|
| `init <path>` | создать репозиторий (`--bare` — bare) |
| `clone <url> <path>` | клонировать (`git://` и локальные пути; авторизация — позже) |
| `status` | статус вида porcelain (`--json`) |
| `log` | история (`-n/--limit`, `--skip`, `--from <rev>`, `--path <file>`, `--json`) |
| `branches` | ветки (`--no-remote`) |
| `show [rev]` | сведения о коммите (по умолчанию HEAD) |
| `diff <rev1> [rev2] [file]` | unified patch между ревизиями (`--name-only` — только файлы; без `rev2` — против рабочего каталога) |
| `trust <path>` | внести репозиторий в `safe.directory` глобального конфига |

Коды возврата: `0` — успех, `1` — ошибка git, `2` — ошибка использования,
`3` — вне репозитория.

### Единственный экземпляр

Повторный запуск завершается сразу (код 0): экземпляр определяется через
`QLockFile` (`/tmp/tortoisegit-TortoiseGit.lock`); «зависший» замок после
падения или kill процесса снимается автоматически по PID. Передача команд
работающему instance через D-Bus ещё не реализована.

## GUI (Phase 3)

`tortoisegit` без `--action` открывает главное окно:

- `File → Open Repository...` (Ctrl+O) — выбор рабочей копии (подкаталог
  репозитория тоже подходит — ищется ближайший родительский каталог);
- `Git → Show Log` (Ctrl+L) — таблица коммитов (hash, дата, автор, сообщение)
  с панелью деталей и выбором лимита; двойной клик по коммиту открывает его
  diff относительно первого родителя;
- `Git → Show Status` (Ctrl+T) — состояние рабочего дерева и список
  локальных/удалённых веток;
- `Git → Show Diff` (Ctrl+D) — список изменённых файлов и unified patch;
  можно указать любые две ревизии, пустое поле `To:` означает рабочее дерево;
- `Help → About`.

Все обращения к git идут через `gitcore` из фонового потока (QtConcurrent) и
не блокируют UI.

Режим диалога (аналогичен CLI TortoiseGit под Windows):

```bash
./build/bin/tortoisegit --action log    --path ~/src/demo
./build/bin/tortoisegit --action status --path ~/src/demo
./build/bin/tortoisegit --action diff   --path ~/src/demo
```

## Конфигурация и логи

- **Конфигурация:** `$XDG_CONFIG_HOME/TortoiseGit/config.ini`
  (по умолчанию `~/.config/TortoiseGit/config.ini`), формат INI; секция
  `[General]`: `LogLimit` (коммитов в Log, `0` — все), `DefaultRepoPath`
  (стартовый каталог «Open Repository»). Редактируется в
  `File → Settings...` (Ctrl+, / Ctrl+Shift+P).
- **Логи:** `$XDG_DATA_HOME/TortoiseGit/logs/`
  (по умолчанию `~/.local/share/TortoiseGit/logs/`); до создания каталога логов
  вывод идёт только в stderr.

## Известные ограничения текущей стадии

1. Из диалогов реализованы Log, Status, Diff и Settings; commit, push/pull,
   blame и разрешение конфликтов пока нет. Просмотр diff — простой
   `QPlainTextEdit` (unified patch), Scintilla-вьюдер отложен.
2. `install(DIRECTORY resources ...)` в корневом CMake защищён `if(EXISTS)`:
   без каталога `resources/` установка проходит, но ресурсов не устанавливает.
3. Сетевые удалённые репозитории с авторизацией (`https`/`ssh`) не
   поддерживаются: встроенная сборка libgit2 собрана без SSH/HTTPS-бэкендов
   (см. `cmake/BuildLibgit2.cmake`); работают локальные пути и `git://`.
4. Тесты `gitcore_tests` требуют установленный в системе `git` (фикстуры
   создают репозитории через `git init/commit`, чтобы проверять gitcore
   независимо от libgit2).

## Troubleshooting

- **`Could not find a package configuration file provided by "Qt6"`**
  укажите путь вручную: `cmake -S . -B build -DQt6_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt6`.
- **`qt.qpa.plugin: Could not load the Qt platform plugin "xcb"`** —
  нет X11/Wayland-сессии; запускайте с `QT_QPA_PLATFORM=offscreen` (или
  `minimal`).
- **Ошибки moc / vtable** — см. примечание про `AUTOMOC` выше.
- **Пустой каталог логов** — каталог логов пока не создаётся автоматически,
  сообщения пишутся в stderr.
- **`GIT_USE_NSEC defined but unknown struct stat nanosecond type`** при
  сборке встроенного libgit2 — пробы конфигурации libgit2 должны запускаться
  с `-D_GNU_SOURCE`; это обеспечивает `cmake/BuildLibgit2.cmake`
  (`CMAKE_REQUIRED_DEFINITIONS`). После правки CMake очистите закэшированные
  пробы: `cmake -B build -U "HAVE_STRUCT_STAT_*"` и пере-configure.
- **`the repository is owned by another user` (GIT_EOWNER)** — сработала
  защита `safe.directory`; выполните `tgit-cli trust <путь>` или добавьте
  запись в `~/.gitconfig` вручную.

## Статус портирования

- [x] Phase 1 — инфраструктура (`CrossPlatform/`: Path, FileSystem, Logging,
      ConfigStore, Thread), сборка каркаса приложения
- [x] Phase 2 — Git-слой (libgit2), консольные утилиты: `gitcore`
      (repository/log/status/branches/diff/clone, safe.directory), `tgit-cli`,
      25 модульных тестов (ctest)
- [x] Phase 3 — GUI на Qt: главное окно (открытие/закрытие репозитория,
      ветка в статус-баре), диалоги Log (история + детали + diff по двойному
      клику), Status (рабочее дерево + ветки), Diff (список файлов +
      unified patch), Settings; интеграция `gitcore` в GUI, фоновая загрузка
      через QtConcurrent, INI-парсинг конфига (`CrossPlatform/ConfigStore`)
- [ ] Осталось в Phase 3: авторизованные удалённые репозитории (SSH/HTTPS),
      Scintilla-вьюдер diff, диалоги commit/push/pull, передача команд
      через D-Bus
- [ ] Phase 4 — интеграция с файловыми менеджерами (Nautilus/Dolphin)
- [ ] Phase 5 — пакеты (AppImage, Flatpak, Deb)

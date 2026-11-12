# TortoiseGit: сборка и запуск под Linux

Инструкция для текущей стадии портирования (Phase 3 — GUI на Qt: главное окно
и диалоги Log/Status/Diff/Settings/Commit/Push-Pull поверх git-слоя `gitcore`
на libgit2 и инфраструктурного слоя Phase 1: CMake + Qt6, утилиты
`CrossPlatform/`). Авторизованные удалённые репозитории (SSH/HTTPS),
Scintilla-вьюдер diff и передача команд через D-Bus включены в эту стадию.
Актуальный статус — в конце документа.

## Требования

| Компонент | Минимум | Примечание |
|---|---|---|
| GCC / Clang | C++20 (GCC ≥ 10 / Clang ≥ 12) | |
| CMake | 3.16 | |
| Qt6 (Core, Gui, Widgets, Network, Concurrent) | 6.2+ | Qt5 допускается как fallback, но не тестировался |
| Qt6 DBus | 6.2+ | опционально: перенаправление повторных запусков к работающему экземпляру |
| OpenSSL (dev) | 1.1+ | опционально: HTTPS для авторизованных ремоутов |
| libssh2 (dev) | 1.9+ | опционально: нативный SSH; без него используется клиент `ssh` |
| QScintilla (Qt6) | 2.13+ | опционально: вьюдер diff; без неё работает Qt-виджет |

Системный `libgit2` опционален: при его отсутствии собирается встроенный
подмодуль `ext/libgit2` (рекомендуется — версия проверена для этого проекта).
Для тестов нужен `ext/googletest`, для JSON-вывода `tgit-cli` — заголовочный
`ext/json` (nlohmann). Подмодули инициализируются командой из раздела
«Сборка»; `libgit2-dev` из пакетов дистрибутива ставить не обязательно.

Все опциональные компоненты определяются автоматически: отсутствие пакета
не ломает сборку, а лишь отключает соответствующую возможность (что именно
подключено — пишет configure).

### Ubuntu / Debian / Mint

```bash
sudo apt install \
    build-essential cmake git \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
    libssl-dev libssh2-1-dev libqscintilla2-qt6-dev
```

### Fedora / RHEL

```bash
sudo dnf install \
    gcc-c++ cmake git \
    qt6-qtbase-devel qt6-qttools-devel \
    openssl-devel libssh2-devel
# QScintilla для Qt6 в основных репозиториях Fedora отсутствует;
# без неё вьюдер diff работает на Qt-виджете (TGIT_WITH_SCINTILLA=OFF)
```

### Arch Linux

```bash
sudo pacman -S \
    base-devel cmake git \
    qt6-base qt6-tools \
    openssl libssh2 qscintilla-qt6
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

# модульные тесты (53 gitcore + 4 ConfigStore)
cd build && ctest
```

Результат: `build/bin/tortoisegit` (GUI), `build/bin/tgit-cli`
(консольный доступ к git), `build/bin/gitcore_tests` и
`build/bin/configstore_tests` (тесты).

### Опции сборки

| Опция | Значение по умолчанию | Назначение |
|---|---|---|
| `TGIT_BUILD_TESTS` | `ON` | сборка `gitcore_tests` / `configstore_tests` |
| `TGIT_WITH_HTTPS` | `ON` | HTTPS встроенного libgit2 через OpenSSL; автоматически выключается, если OpenSSL не найден |
| `TGIT_WITH_SSH` | `AUTO` | `AUTO` — libssh2, иначе клиент `ssh`; `LIBSSH2` / `EXEC` — явно; `OFF` — без SSH |
| `TGIT_AUTH_ROOT` | пусто | дополнительный префикс с заголовками и библиотеками OpenSSL/libssh2 |
| `TGIT_WITH_SCINTILLA` | `AUTO` | `AUTO` — QScintilla при наличии; `ON` — требовать; `OFF` — Qt-виджет |
| `TGIT_QSCINTILLA_ROOT` | пусто | префикс с QScintilla (пакеты кладут заголовки в `.../qt6/Qsci`) |

Тип сборки по умолчанию — `Debug`; доступные значения `CMAKE_BUILD_TYPE`:
`Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`.

> Классы с `Q_OBJECT` обрабатываются автоматически: в `src/CMakeLists.txt`
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
| `-s, --action <action>` | режим диалога: `log`, `status`, `diff`, `commit`, `push`, `pull` (см. «GUI») |
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
./build/bin/tgit-cli -C ~/src/demo add && ./build/bin/tgit-cli -C ~/src/demo commit -m "prepare"
./build/bin/tgit-cli -C ~/src/demo pull
./build/bin/tgit-cli -C ~/src/demo push         # текущая ветка — в ветку с тем же именем
./build/bin/tgit-cli features                   # какие транспорты в сборке
./build/bin/tgit-cli trust /mnt/other-user/repo # добавляет в safe.directory
```

| Команда | Назначение |
|---|---|
| `init <path>` | создать репозиторий (`--bare` — bare) |
| `clone <url> <path>` | клонировать (`git://`, локальные пути, а также `https`/`ssh`, если сборка содержит нужный транспорт) |
| `status` | статус вида porcelain (`--json`) |
| `log` | история (`-n/--limit`, `--skip`, `--from <rev>`, `--path <file>`, `--json`) |
| `branches` | ветки (`--no-remote`) |
| `show [rev]` | сведения о коммите (по умолчанию HEAD) |
| `diff <rev1> [rev2] [file]` | unified patch между ревизиями (`--name-only` — только файлы; без `rev2` — против рабочего каталога) |
| `add [path...]` | внести в индекс (по умолчанию — всё) |
| `unstage [path...]` | убрать из индекса (по умолчанию — всё) |
| `commit -m <msg>` | создать коммит из индекса (`--all` — сначала добавить все изменения) |
| `remote` | список настроенных ремоутов |
| `features` | транспорты, встроенные в libgit2 (HTTPS/SSH) |
| `fetch [refspec...]` | получить refs ремоута |
| `push [refspec...]` | отправить refs (без refspec — текущая ветка) |
| `pull` | получить и перемотать текущую ветку; при расхождении — код 4 и подсказка |
| `trust <path>` | внести репозиторий в `safe.directory` глобального конфига |

Коды возврата: `0` — успех, `1` — ошибка git, `2` — ошибка использования,
`3` — вне репозитория, `4` — ветки разошлись (pull ничего не сливает).

Для `clone`/`fetch`/`push`/`pull` есть `--remote <name>`, `--username`,
`--password`, `--ssh-key <file>`, `--passphrase`. Без них в интерактивном
терминале задаётся вопрос по тому, что требует ремоут, а неинтерактивный
запуск сначала отдаёт приоритет ssh-agent и credential helper'ам и
описывает нехватку учётных данных подсказкой (а не кодом ошибки libgit2).

Если транспорт недоступен в сборке, `clone`/`push`/`pull` сообщают об этом
прямо и предлагают нужную опцию CMake, а не бесполезную ошибку libgit2.

### Единственный экземпляр и D-Bus

Повторный запуск завершается сразу (код 0): экземпляр определяется через
`QLockFile` (`/tmp/tortoisegit-TortoiseGit.lock`); «зависший» замок после
падения или kill процесса снимается автоматически по PID.

Если Qt DBus доступен при сборке, второй запуск не просто выходит, а
передаёт команду работающему экземпляру: вызывается метод
`Activate(action, path, url, revision)` интерфейса
`org.tortoisegit.TortoiseGit` (объект `/org/tortoisegit/TortoiseGit`,
session bus), и соответствующий диалог открывается в уже запущенном окне.
Без модуля DBus сохраняется прежнее поведение — тихий выход второго
экземпляра.

## GUI (Phase 3)

`tortoisegit` без `--action` открывает главное окно:

- `File → Open Repository...` (Ctrl+O) — выбор рабочей копии (подкаталог
  репозитория тоже подходит — ищется ближайший родительский каталог);
- `File → Settings...` (Ctrl+,), `File → Exit` (Ctrl+Q);
- `Git → Show Log` (Ctrl+L) — таблица коммитов (hash, дата, автор, сообщение)
  с панелью деталей и выбором лимита; двойной клик по коммиту открывает его
  diff относительно первого родителя;
- `Git → Show Status` (Ctrl+T) — состояние рабочего дерева и список
  локальных/удалённых веток;
- `Git → Show Diff` (Ctrl+D) — список изменённых файлов и патч; можно указать
  любые две ревизии, пустое поле `To:` означает рабочее дерево;
- `Git → Commit...` (Ctrl+M) — список изменённых и новых файлов с чекбоксами
  stage/unstage (плюс `Stage All` / `Unstage All` и работа с выделением),
  поле сообщения и коммит через `GitCore::CreateCommit`; `Ctrl+Return`
  отправляет коммит из поля сообщения. Диалог показывает, под какой
  идентифичностью будет создан коммит, и закрывается после успеха;
- `Git → Push...` (Ctrl+P) и `Git → Pull...` (Ctrl+Shift+P) — выбор ремоута,
  отправка/получение текущей ветки с логом прогресса передачи. Pull при
  расхождении веток ничего не сливает и явно предлагает merge или rebase;
- `Help → About`.

Если ремоут запрашивает учётные данные, открывается `CredentialsDialog`:
логин и пароль/токен для `https`, путь к приватному ключу и его пароль для
`ssh`, либо работа через ssh-agent. Отказ от диалога не прерывает передачу —
libgit2 по-прежнему может использовать ssh-agent или credential helper.
Полученные за время одного диалога учётные данные кешируются в памяти и на
диск не пишутся.

Все обращения к git идут через `gitcore` из фонового потока (QtConcurrent) и
не блокируют UI. Вьюдер diff использует QScintilla, если она найдена;
иначе — встроенный Qt-виджет (unified patch). Какой backend выбран, видно из
сообщения `Diff viewer backend: Scintilla|Qt` в логе запуска.

Режим диалога (аналогичен CLI TortoiseGit под Windows):

```bash
./build/bin/tortoisegit --action log    --path ~/src/demo
./build/bin/tortoisegit --action status --path ~/src/demo
./build/bin/tortoisegit --action diff   --path ~/src/demo --revision HEAD~1
./build/bin/tortoisegit --action commit --path ~/src/demo
./build/bin/tortoisegit --action push   --path ~/src/demo
./build/bin/tortoisegit --action pull   --path ~/src/demo
```

## Конфигурация и логи

- **Конфигурация:** `$XDG_CONFIG_HOME/TortoiseGit/config.ini`
  (по умолчанию `~/.config/TortoiseGit/config.ini`), формат INI; секция
  `[General]`: `LogLimit` (коммитов в Log, `0` — все), `DefaultRepoPath`
  (стартовый каталог «Open Repository»). Редактируется в
  `File → Settings...` (Ctrl+,).
- **Логи:** `$XDG_DATA_HOME/TortoiseGit/logs/`
  (по умолчанию `~/.local/share/TortoiseGit/logs/`); каталог логов пока не
  создаётся, поэтому вывод идёт только в stderr (см. «Известные ограничения»).

## Известные ограничения текущей стадии

1. Из диалогов не реализованы blame и разрешение конфликтов; нет и
   интеграции с файловыми менеджерами (Phase 4). В коммите нет amend,
   подписи GPG и частичного staging по строкам — только whole-file.
2. `install(DIRECTORY resources ...)` в корневом CMake защищён `if(EXISTS)`:
   без каталога `resources/` установка проходит, но ресурсов не устанавливает.
3. Авторизованные ремоуты зависят от сборки: HTTPS требует OpenSSL,
   нативный SSH — libssh2. Если libssh2 нет, SSH работает через клиент `ssh`
   (ключи и ssh-agent работают, но пароль от ключа ввести нельзя —
   нужен ssh-agent). Возможности библиотеки определяются в рантайме
   (`GitCore::SupportsHttps/SupportsSsh`), поэтому отсутствующий транспорт
   не мешает локальным репозиториям и `git://`.
4. Учётные данные живут только в памяти открытого диалога: интеграции с
   gnome-libsecret и credential helper'ами нет.
5. Тесты `gitcore_tests` требуют установленный в системе `git` (фикстуры
   создают репозитории через `git init/commit`, чтобы проверять gitcore
   независимо от libgit2).
6. Каталог логов не создаётся: `CLogging::Init` формирует путь, но не
   вызывает создание каталога, поэтому файл лога не появляется.
7. На offscreen-платформе Qt пишет отладочный шум `qt.widgets.painting`;
   он не связан с кодом проекта и отключается так:
   `QT_LOGGING_RULES='qt.widgets.painting=false'`.

## Troubleshooting

- **`Could not find a package configuration file provided by "Qt6"`**
  укажите путь вручную: `cmake -S . -B build -DQt6_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt6`.
- **`qt.qpa.plugin: Could not load the Qt platform plugin "xcb"`** —
  нет X11/Wayland-сессии; запускайте с `QT_QPA_PLATFORM=offscreen` (или
  `minimal`).
- **Ошибки moc / vtable** — см. примечание про `AUTOMOC` выше.
- **Пустой каталог логов** — каталог логов пока не создаётся автоматически,
  сообщения пишутся в stderr.
- **`unsupported URL protocol` / сообщение о недоступном транспорте** —
  сборка без нужного бэкенда; смотрите строки `libgit2 HTTPS transport:` и
  `libgit2 SSH transport:` из вывода configure и опции `TGIT_WITH_HTTPS`,
  `TGIT_WITH_SSH`, `TGIT_AUTH_ROOT`.
- **SSH просит пароль от ключа и падает** — используется транспорт `EXEC`
  (клиент `ssh`): разблокируйте ключ через `ssh-agent` или соберите с
  libssh2 (`TGIT_WITH_SSH=LIBSSH2`).
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
      модульные тесты (ctest)
- [x] Phase 3 — GUI на Qt: главное окно (открытие/закрытие репозитория,
      ветка в статус-баре), диалоги Log (история + детали + diff по двойному
      клику), Status (рабочее дерево + ветки), Diff (список файлов +
      патч), Settings; интеграция `gitcore` в GUI, фоновая загрузка
      через QtConcurrent, INI-парсинг конфига (`CrossPlatform/ConfigStore`)
- [x] Осталось в Phase 3 — выполнено:
      авторизованные ремоуты SSH/HTTPS + диалог учётных данных,
      Scintilla-вьюдер diff с Qt-фолбэком, диалоги commit и push/pull
      (`GitCore::Stage/CreateCommit`, `GitCore::Push/Pull` с прогрессом),
      передача команд через D-Bus; 57 тестов ctest
- [ ] Phase 4 — интеграция с файловыми менеджерами (Nautilus/Dolphin),
      blame, разрешение конфликтов
- [ ] Phase 5 — пакеты (AppImage, Flatpak, Deb)

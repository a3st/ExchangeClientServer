# Клиент-Сервер Биржа

## Возможности

### Сервер

1. Поддерживает подключения нескольких клиентов одновременно.
2. Принимает заявки на покупку или продажу долларов за рубли от разных клиентов.
3. Даёт возможность просмотра баланса клиента.

### Клиент (Простое консольное приложение)

1. Подключается к серверу и реализует все его возможности.

### Торговая логика

1. Торговая заявка содержит объём, цену и сторону (покупка/продажа).
2. Если две заявки пересекаются по цене — для сделки выбирается цена более ранней заявки.
3. Если заявка пересекается с несколькими заявками по другой стороне — приоритет в исполнении отдаётся заявкам с максимальной ценой для заявок на покупку и с минимальной ценой для заявок на продажу.
4. Частичное исполнение заявки.
5. Торговая заявка активна до тех пор, пока по ней есть активный объём.
6. Баланс клиента не ограничен — он может торговать в минус.

### Дополнительно

1. Клиент и сервер используют протокол SRP-6 для аутентификации пользователя.
2. В качестве СУБД сервер использует - SQLite.
3. Серверная архитектура позволяет быстро добавлять различные функции.
4. Для простоты клиент/сервер создает TCP пакет в виде JSON сообщения.

## Сборка

### Зависимости

- [Boost](https://github.com/boostorg/boost)
- [Botan](https://github.com/randombit/botan)
- [GoogleTest](https://github.com/google/googletest)
- [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp)
- [spdlog](https://github.com/gabime/spdlog)
- [argh](https://github.com/adishavit/argh)

### Инструкция

Для сборки под Windows используйте пакетный менеджер [vcpkg](https://github.com/microsoft/vcpkg)

```bash
vcpkg install boost botan gtest sqlitecpp spdlog argh --triplet=x64-windows-static-md
```

* CMake 3.25.1+
* C++20 compatible compiler
* vcpkg
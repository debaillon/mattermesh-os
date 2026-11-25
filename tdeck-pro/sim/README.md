# Host tests

`test_pack_reader.cpp` checks `services/pack_reader.cpp` + `services/pack_loader.cpp` (the Portugal/travel-pack
search service) against `fixtures/`: a real pack built by the Python packbuilder and an "oracle" of query
results produced by its reference reader (`packlib/reader.py`), which FORMAT.md treats as the executable
specification the device must match byte-for-byte. Regenerate both with
`python3 gen_test_fixture.py` from `Travel LLM/packbuilder/` after changing the pack format or its test data.

Build and run:
```
g++ -std=c++17 -O1 -Isrc -I.pio/libdeps/mattermesh-free/ArduinoJson/src \
    src/services/pack_reader.cpp src/services/pack_loader.cpp sim/test_pack_reader.cpp -o /tmp/test_pack_reader
/tmp/test_pack_reader
```
(run from `tdeck-pro/`; the ArduinoJson path needs a prior `pio run` to populate `.pio/libdeps/`)

`pack_reader.cpp`/`.h` take no Arduino/SD/ESP-IDF includes - only the `pack::IO` interface and `pack::Tokenizer`
struct - so this is the same code the device runs, not a reimplementation. `pack_sd.cpp` (the SD-backed `pack::IO`
and the `device::pack*` facade main.cpp's `pack open|search|near` CLI commands call) is device-only and isn't
covered here; it was checked by flashing and running those commands against a missing pack folder (the
graceful-failure path - there is no on-device pack to search yet).

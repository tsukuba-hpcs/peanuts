# peanuts.hpp, libpeanuts_c
## install
```bash
make build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/install
cmake --build . -j
cmake --install .

# don't do like this
# cmake --install . --prefix=/path/to/install
```

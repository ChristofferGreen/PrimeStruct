# PrimeStruct Native Target: Spinning Cube Host

This sample hosts the shared spinning-cube simulation source from
`examples/web/spinning_cube/cube.prime` through a native desktop glue program.

## Files
- `main.cpp`: minimal native host glue that executes the compiled cube sample
  and validates the expected deterministic exit code.

## Local Build Steps
1. Build the shared cube sample to a native binary:
   - `./primec --emit=native ../../web/spinning_cube/cube.prime -o ./cube_native --entry /main`
2. Build the native host glue:
   - `c++ -std=c++17 main.cpp -o spinning_cube_host`
3. Run host + sample:
   - `./spinning_cube_host ./cube_native`

The host should print `native host verified cube simulation output` and exit 0.

# TetherLRS firmware tools

Scripts that make and check the builds; the firmware itself does not need them to run.

- `tetherlrs_gen_elrs.py`: a hal and an env for every ExpressLRS receiver and Tx module in `../elrs-targets` that
  no hand-written hal covers, from ExpressLRS's hardware descriptions; `tetherlrs_tx_envs.py` makes every receiver
  also a ground radio; `tetherlrs_elrs_products.py` writes the table that tells the tool which build fits which
  product. Run them in that order after updating the submodule; `--check` says whether the files are up to date.
- `run_make_firmwares.py`: every STM32 image, with arm-none-eabi-gcc on the PATH; `run_copy_st_drivers.py` first,
  to put the ST drivers into each target. `run_make_esp_firmwares.py`: every ESP env with PlatformIO.
- `tetherlrs_package.py`: the release files and the manifest the tool reads.
- `st-drivers/`: the ST CMSIS and HAL drivers as submodules.

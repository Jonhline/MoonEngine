# Race screens and mode routing

Race presentation is selected from authored menu projects and mode conditions. Pre-race, pause, results and awards follow different paths for Arcade, licenses, missions and other career events. The inventory records original screen routes; a listed route is not proof of complete native presentation.

## Evidence and limitations

The [development record](notes/RACE_SCREEN_INVENTORY.md) retains source addresses,
experiments, positive and negative cases, and historical limitations.
Consult [Fidelity](FIDELITY.md) for each comparison's grade and scope.
Earlier entries may be superseded by later results.

## Screen Setting and scene color correction

The original scene draw (`0033F2E8 +0x458..0x49C`) selects race or replay display options through `003EFEA8` or `003EFEE8`, builds correction parameters, skips the identity transform, and corrects the scene before display overlays. These options are separate four-element arrays: brightness, contrast, saturation, and color temperature. The pause callback (`003C3260`) changes only brightness or contrast and preserves the other two entries.

The native presentation boundary reads the active profile's matching array, applies the original channel LUT and GS color mixing to a retained GPU capture, and then draws the HUD and pause controls. Zero options bypass the correction. Captures and correction use the same rounded viewport and expand across the widescreen scene. ResultLicense applies its own saturation transition to its uncorrected capture, avoiding a second brightness/contrast pass.

`screen_setting_oracle` compares the original race/replay getters and six parameters bit for bit (E). `screen_setting` exercises the actual race/replay presentation call, pause callbacks, profile persistence and GPU pixels in both display ratios with a one-byte tolerance (C). Both include deliberate failing cases. These tests certify the color correction boundary; they do not certify complete PS2 scene rasterization.

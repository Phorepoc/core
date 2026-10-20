# phepowerkeyplugin (OURS)

Control Panel (GS) page "Power key": what a short / a long (2 s) press of the power key does.
Two radio-button settings in CenRep 0xE70157B0 (keys 1 / 2; values 0 Power menu, 1 Lock / unlock, 2 Switch off) -- SysAp reads
them at every press (`src/phorepoc-arm/board/cenrep/e70157b0.txt`).

- DLL UID3 0xE70157B1, ECOM implementation 0xE70157B2 (= the view id), interface 0x10207236.
- Registered under KGSGenPluginUid 0x10207237 (the Device / General folder, as gssensorplugin), order "60".
- Built from the registry: `ARM_VFP=1 py emitter/build_mmp.py phepowerkeyplugin`; it ships in the ARM ROM like any registered component.
- Modelled on GSAutoKeyguardPlugin (CGSBaseView / CGSBaseContainer / CGSListBoxItemTextArray + CAknRadioButtonSettingPage).

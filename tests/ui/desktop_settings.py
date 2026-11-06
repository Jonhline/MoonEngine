"""Real executable, isolated preferences: apply/cancel, presets and persistence.
Pages as their Options tiles open them (MOON_SETTINGS_PAGE): 3 Controls, 5 Presets."""
import pathlib, subprocess, sys, tempfile, re, os
exe, disc, output = map(pathlib.Path, sys.argv[1:4])
output.mkdir(exist_ok=True, parents=True)
with tempfile.TemporaryDirectory(dir=output) as directory:
    config=pathlib.Path(directory)/'options.ini'
    config.write_text('controls.desktop_menu=0\ninterface.regulation_separators=0\npreset=Classic\ngraphics.ui_filter=2\n')
    def run(name, inputs='', page=3):
        args=[str(exe.resolve()),str(disc.resolve()),'option','ScreenRoot','--video-settings',
              '--config-file',str(config),'--shot',str(output/(name+'.png'))]
        if inputs: args+=['--input',inputs]
        env=dict(os.environ, MOON_WINDOW_HIDDEN="1", SDL_AUDIODRIVER="dummy", MOON_SETTINGS_PAGE=str(page),
                 MOON_OVERLAY=str(pathlib.Path(directory)/'no-mods'))
        result=subprocess.run(args,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=45)
        log=result.stdout+result.stderr
        (output/(name+'.log')).write_text(log,encoding='utf-8')
        assert result.returncode==0 and 'undefined' not in log and 'call of nil' not in log,log
        assert (output/(name+'.png')).stat().st_size>1000
        return log
    log=run('classic')
    # Controls: Desktop Input, Input Hints, Smooth Cursor, Hover Feedback, Main Menu,
    # Map Edge Pan, Compact Car Strip, Graphics Swap Key, Controller Tiles
    ROWS=9
    def change(index, direction='up'):
        return ['down']*index+['accept',direction,'accept']+['down']*(ROWS-index)+['accept']
    assert 'video settings: page Controls' in log,log
    assert 'choice SmoothCursor/OptionMenu index=0' in log,log
    assert 'choice HoverFeedback/OptionMenu index=0' in log,log
    assert 'choice MainMenuOverview/OptionMenu index=0' in log,log
    assert 'choice CompactCars/OptionMenu index=0' in log,log
    assert 'RegulationSeparators' not in log,log
    assert 'choice DesktopInput/OptionMenu index=0' in log,log
    assert 'choice MapEdgePan/OptionMenu index=0' in log,log
    assert 'choice InputHints/OptionMenu index=0' in log,log
    run('enable',','.join(change(0,'down')))
    assert 'controls.desktop_menu=1' in config.read_text(),config.read_text()
    log=run('reload')
    assert 'choice DesktopInput/OptionMenu index=1' in log,log
    assert 'choice PCPreset/OptionMenu index=3' in log,log  # Modified
    before=config.read_bytes()
    run('cancel',','.join(['accept','up','accept','back']))
    assert config.read_bytes()==before
    # Presets: PC Preset, Preset Files, Save Transfer
    run('classic_apply',','.join(['accept','up','up','up','accept']+['down']*3+['accept']),5)
    data=config.read_text()
    assert 'graphics.antialiasing=0' in data and 'graphics.texture_smoothing=0' in data,data
    assert 'controls.desktop_menu=0' in data and 'preset=Classic' in data and 'modified=0' in data,data
    # Enhanced turns Preset Files on: Load/Save appear; Save, then down, is OK
    run('enhanced_apply',','.join(['accept','down','accept']+['down']*3+['right','down','accept']),5)
    data=config.read_text()
    assert 'graphics.antialiasing=4' in data and 'graphics.render_scale=2' in data and 'graphics.texture_smoothing=1' in data,data
    assert 'controls.desktop_menu=1' in data and 'preset=Enhanced' in data and 'modified=0' in data,data
    assert 'controls.gtmode_edge_pan=1' in data and 'interface.input_hints=1' in data,data
    assert 'interface.main_menu_overview=1' in data,data
    assert 'interface.smooth_mouse_cursor=1' in data and 'interface.mouse_hover_feedback=1' in data,data
    assert 'interface.compact_car_strip=1' in data,data
    run('smooth_off',','.join(change(2)))
    assert 'interface.smooth_mouse_cursor=0' in config.read_text()
    run('feedback_off',','.join(change(3)))
    assert 'interface.mouse_hover_feedback=0' in config.read_text()
    run('overview_off',','.join(change(4)))
    assert 'interface.main_menu_overview=0' in config.read_text()
    run('hints_off',','.join(change(1)))
    data=config.read_text()
    assert 'controls.desktop_menu=1' in data and 'interface.input_hints=0' in data and 'modified=1' in data,data
    run('compact_off',','.join(change(6)))
    assert 'interface.compact_car_strip=0' in config.read_text(),config.read_text()
    run('map_off',','.join(change(5)))
    data=config.read_text()
    assert 'controls.desktop_menu=1' in data and 'controls.gtmode_edge_pan=0' in data and 'interface.input_hints=0' in data,data

print('Desktop settings: both values, apply, cancel, reload, modified, Classic and Enhanced passed.')

"""Real executable, isolated preferences: apply/cancel, presets and persistence."""
import pathlib, subprocess, sys, tempfile, re, os
exe, disc, output = map(pathlib.Path, sys.argv[1:4])
output.mkdir(exist_ok=True, parents=True)
with tempfile.TemporaryDirectory(dir=output) as directory:
    config=pathlib.Path(directory)/'options.ini'
    config.write_text('controls.desktop_menu=0\ninterface.regulation_separators=0\npreset=Classic\ngraphics.ui_filter=2\n')
    def run(name, inputs=''):
        args=[str(exe.resolve()),str(disc.resolve()),'option','ScreenRoot','--video-settings',
              '--config-file',str(config),'--shot',str(output/(name+'.png'))]
        if inputs: args+=['--input',inputs]
        result=subprocess.run(args,env=dict(os.environ, MOON_WINDOW_HIDDEN="1"),capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=45)
        log=result.stdout+result.stderr
        (output/(name+'.log')).write_text(log,encoding='utf-8')
        assert result.returncode==0 and 'undefined' not in log and 'call of nil' not in log,log
        assert (output/(name+'.png')).stat().st_size>1000
        return log
    log=run('classic')
    def tab(index):
        return ['up','up']+['left']*(2-index)+['accept']
    def change(index):
        return tab(2)+['down']*index+['accept','up','accept']+['down']*(6-index)+['accept']
    assert 'choice SmoothCursor/OptionMenu index=0' in log,log
    assert 'choice HoverFeedback/OptionMenu index=0' in log,log
    assert 'choice MainMenuOverview/OptionMenu index=0' in log,log
    assert 'choice CompactCars/OptionMenu index=0' in log,log
    assert 'RegulationSeparators' not in log,log
    assert 'choice DesktopInput/OptionMenu index=0' in log,log
    assert 'choice MapEdgePan/OptionMenu index=0' in log,log
    assert 'choice InputHints/OptionMenu index=0' in log,log
    run('enable',','.join(tab(2)+['accept','down','accept']+['down']*6+['accept']))
    assert 'controls.desktop_menu=1' in config.read_text(),config.read_text()
    log=run('reload')
    assert 'choice DesktopInput/OptionMenu index=1' in log,log
    assert 'choice PCPreset/OptionMenu index=2' in log,log
    before=config.read_bytes()
    run('cancel',','.join(tab(2)+['accept','up','accept','back']))
    assert config.read_bytes()==before
    run('classic_apply',','.join(['up','accept','up','up','accept']+['down']*5+['accept']))
    data=config.read_text()
    assert 'controls.desktop_menu=0' in data and 'preset=Classic' in data and 'modified=0' in data,data
    run('enhanced_apply',','.join(['up','accept','down','accept']+['down']*5+['accept']))
    data=config.read_text()
    assert 'controls.desktop_menu=1' in data and 'preset=Enhanced' in data and 'modified=0' in data,data
    assert 'controls.gtmode_edge_pan=1' in data and 'interface.input_hints=1' in data,data
    assert 'interface.main_menu_overview=1' in data,data
    assert 'interface.smooth_mouse_cursor=1' in data and 'interface.mouse_hover_feedback=1' in data,data
    assert 'interface.compact_car_strip=1' in data,data
    run('smooth_off',','.join(change(4)))
    assert 'interface.smooth_mouse_cursor=0' in config.read_text()
    run('feedback_off',','.join(change(5)))
    assert 'interface.mouse_hover_feedback=0' in config.read_text()
    run('overview_off',','.join(change(3)))
    assert 'interface.main_menu_overview=0' in config.read_text()
    run('hints_off',','.join(change(2)))
    data=config.read_text()
    assert 'controls.desktop_menu=1' in data and 'interface.input_hints=0' in data and 'modified=1' in data,data
    run('rendering_layout','r1')
    for row,key in [(5,'interface.compact_car_strip')]:
        run('readability_off_'+str(row),','.join(['r1']+['down']*row+['accept','up','accept']+['down']*(6-row)+['accept']))
        assert key+'=0' in config.read_text(),config.read_text()
    run('map_off',','.join(change(1)))
    data=config.read_text()
    assert 'controls.desktop_menu=1' in data and 'controls.gtmode_edge_pan=0' in data and 'interface.input_hints=0' in data,data

print('Desktop settings: both values, apply, cancel, reload, modified, Classic and Enhanced passed.')

// SparkPark builder: the page's state machine, wired to the launcher (launcher.cpp) instead of
// the design reference's timers. make_ui.py puts this in place of the reference's <script>.
//
// Page -> launcher: {cmd:'ready'} (once loaded), {cmd:'pick'} (choose and check a disc),
//                   {cmd:'build'}, {cmd:'play'}, {cmd:'back'} (another disc after a warning),
//                   {cmd:'restore'} (restore a save backup),
//                   {cmd:'state', name} (after each change, for screenshots in test runs).
// Launcher -> page: {type:'init', update, fromExtracted, from, to, backups},
//                   {type:'verifying'}, {type:'verifyProgress', pct},
//                   {type:'verified', warn?}, {type:'discError', reason}, {type:'reset'},
//                   {type:'progress', pct, stage: 'extract'|'recompile'|'link'},
//                   {type:'done'}, {type:'buildError', reason},
//                   {type:'backups', count}, {type:'note', text, error?, backups?}.
(function(){
  var phase='upload', pct=0, stage='extract', error='', retry=false, vpct=-1;
  var warn='', note='', update=null, backups=0;
  var $=function(id){return document.getElementById(id)};
  var face=$('face'), btn=$('mainBtn');
  var capBg=$('capP').style.background;  // the design's caption pill; errors swap in a red one
  var host=window.chrome&&window.chrome.webview;
  var COLORS={blue:['#4A86E0','#1F4A94'],orange:['#EE6A2C','#7A2A0C'],teal:['#35B0A0','#15524B']};
  function send(o){if(host)host.postMessage(o)}
  function show(name,on){document.querySelectorAll('[data-show="'+name+'"]').forEach(function(el){el.style.display=on?'contents':'none'})}
  // SparkPark: "Restore a backup", in the bottom bar's middle, once there are save backups.
  var bar=document.querySelector('.sp-bar'), restore=document.createElement('button');
  restore.textContent='Restore a save backup';
  restore.style.cssText='font:inherit;font-size:16px;font-weight:700;color:#FFF3DC;background:rgba(255,255,255,.12);'+
    'border:2px solid rgba(255,255,255,.35);border-radius:999px;padding:4px 16px;cursor:pointer;display:none';
  restore.addEventListener('click',function(){if(phase!=='verifying'&&phase!=='building')send({cmd:'restore'})});
  if(bar){var mid=bar.lastElementChild;bar.insertBefore(restore,mid);}
  function render(){
    var c=phase==='play'?'teal':(phase==='upload'||phase==='verifying')?'blue':'orange';
    face.style.backgroundColor=COLORS[c][0]; btn.style.background=COLORS[c][1];
    show('isUpload',phase==='upload'); show('isVerifying',phase==='verifying'); show('isBuild',phase==='build');
    show('isBuilding',phase==='building'); show('isPlay',phase==='play');
    $('buildLabel').textContent=retry?'Retry Build':update&&phase==='build'?'Rebuild':'Build Recomp';
    var cap='', amber=false;
    if(error)cap=error;
    else if(note){cap=note;}
    else if(phase==='verifying')cap='Checking your disc…'+(vpct>=0?' '+vpct+'%':'');
    else if(phase==='build'&&warn){cap=warn;amber=true;}
    else if(phase==='build'&&update)cap='Updated to v'+update.to+' · rebuild your game (saves kept)';
    else if(phase==='build')cap='Disc verified · ready to build';
    else if(phase==='upload'&&update)cap='Updated to v'+update.to+' · choose your disc to rebuild (saves kept)';
    else if(phase==='building')cap=stage==='extract'?(update&&update.fromExtracted?'Checking your extracted disc…':'Extracting your disc…'):stage==='link'?'Linking the runtime…':'Recompiling game code…';
    else if(phase==='play')cap='Build complete · ready to play';
    var capP=$('capP');
    var visible=phase!=='upload'||!!error||!!note||!!update;
    var wasHidden=capP.parentNode.style.display==='none';
    show('showCaption',visible);
    if(wasHidden&&visible){capP.style.animation='none';void capP.offsetWidth;capP.style.animation='';}
    capP.style.background=error?'rgba(130,24,10,.82)':amber?'rgba(150,96,0,.85)':capBg;
    show('showCheck',!error&&!amber&&!note&&(phase==='build'&&!update||phase==='play'));
    $('caption').textContent=cap;
    $('pct').textContent=pct; $('bar').style.width=pct+'%';
    $('hint').textContent=phase==='upload'?'Upload ISO':phase==='build'?(retry?'Retry':update?'Rebuild':'Build'):phase==='play'?'Play':'Please wait';
    btn.setAttribute('aria-label',phase==='upload'?'Upload ISO':phase==='verifying'?'Checking disc':phase==='build'?(retry?'Retry Build':update?'Rebuild':'Build Recomp'):phase==='building'?'Building':'Play');
    $('idle').className=(phase==='upload'||phase==='build'||phase==='play')?'sp-idle':'';
    restore.style.display=backups>0&&(phase==='upload'||phase==='build'||phase==='play')?'':'none';
    send({cmd:'state',name:error?(phase==='upload'?'disc-error':'build-error'):warn&&phase==='build'?'disc-warning':
      update&&phase==='build'?'update':phase==='building'?'building-'+stage+(pct>=50?'-half':''):phase});
  }
  function press(){
    note='';
    if(phase==='upload')send({cmd:'pick'});
    else if(phase==='build')send({cmd:'build'});
    else if(phase==='play')send({cmd:'play'});
  }
  function back(){if(phase==='build'&&warn){warn='';send({cmd:'back'});}}
  if(host)host.addEventListener('message',function(e){
    var m=e.data;
    if(m.type==='init'){
      backups=m.backups||0;
      if(m.update){update={from:m.from,to:m.to,fromExtracted:!!m.fromExtracted};phase=m.fromExtracted?'build':'upload';}
      // Test runs (SPARKPARK_TEST_RESTORE): press "Restore a save backup" once the page has settled.
      if(window.SP_TEST_RESTORE&&backups>0)setTimeout(function(){restore.click();},3500);
    }
    else if(m.type==='verifying'){phase='verifying';error='';note='';warn='';retry=false;vpct=-1;}
    else if(m.type==='verifyProgress'){vpct=m.pct;$('caption').textContent='Checking your disc… '+vpct+'%';if(vpct>=50)send({cmd:'state',name:'verifying-half'});return;}
    else if(m.type==='verified'){phase='build';error='';warn=m.warn||'';if(update)update.fromExtracted=false;}
    else if(m.type==='discError'){phase='upload';error=m.reason;warn='';if(update)update.fromExtracted=false;}
    else if(m.type==='reset'){phase='upload';error='';warn='';}
    else if(m.type==='progress'){phase='building';error='';note='';pct=Math.max(0,Math.min(100,Math.floor(m.pct)));stage=m.stage;}
    else if(m.type==='done'){phase='play';pct=100;error='';warn='';retry=false;update=null;}
    else if(m.type==='buildError'){phase='build';error=m.reason;retry=true;}
    else if(m.type==='backups'){backups=m.count;}
    else if(m.type==='note'){note=m.text;if(m.backups!=null)backups=m.backups;if(m.error){error=m.text;note='';}}
    render();
  });
  btn.addEventListener('click',press);
  document.addEventListener('keydown',function(e){
    if(e.key==='Escape'){back();return;}
    if(e.key==='Enter'||e.key===' '){if(document.activeElement!==btn&&document.activeElement!==restore){e.preventDefault();press();}}
  });
  // A controller's A button (standard mapping: button 0) presses the button, as Enter does; B
  // (button 1) goes back after a disc warning, as Esc does.
  var padDown=true, padBack=true;  // ignore buttons already held when the page opens
  function pollPads(){
    var pads=navigator.getGamepads?navigator.getGamepads():[], a=false, b=false;
    for(var i=0;i<pads.length;i++){var p=pads[i];if(!p||!p.buttons)continue;if(p.buttons[0]&&p.buttons[0].pressed)a=true;if(p.buttons[1]&&p.buttons[1].pressed)b=true;}
    if(a&&!padDown)press();
    if(b&&!padBack)back();
    padDown=a; padBack=b;
    requestAnimationFrame(pollPads);
  }
  requestAnimationFrame(pollPads);
  // The stage is a fixed 1280x720 scene: scale it to fit the window, as a TV would.
  var stage0=$('spStage');
  function fit(){var s=Math.min(innerWidth/1280,innerHeight/720);stage0.style.transform='scale('+s+')';}
  addEventListener('resize',fit); fit();
  // Test runs (SPARKPARK_AUTOPRESS): press the button once each state has settled.
  if(window.SP_AUTOPRESS){
    var last='';
    setInterval(function(){
      var key=phase+'|'+error+'|'+warn;
      if(key!==last&&(phase==='upload'||phase==='build'||phase==='play')){last=key;setTimeout(press,window.SP_AUTOPRESS);}
    },250);
  }
  render();
  send({cmd:'ready'});
})();

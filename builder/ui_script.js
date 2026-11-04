// SparkPark builder: the page's state machine, wired to the launcher (launcher.cpp) instead of
// the design reference's timers. make_ui.py puts this in place of the reference's <script>.
//
// Page -> launcher: {cmd:'pick'} (choose and check a disc), {cmd:'build'}, {cmd:'play'},
//                   {cmd:'state', name} (after each change, for screenshots in test runs).
// Launcher -> page: {type:'verifying'}, {type:'verified'}, {type:'discError', reason},
//                   {type:'progress', pct, stage: 'extract'|'recompile'|'link'},
//                   {type:'done'}, {type:'buildError', reason}.
(function(){
  var phase='upload', pct=0, stage='extract', error='', retry=false;
  var $=function(id){return document.getElementById(id)};
  var face=$('face'), btn=$('mainBtn');
  var host=window.chrome&&window.chrome.webview;
  var COLORS={blue:['#4A86E0','#1F4A94'],orange:['#EE6A2C','#7A2A0C'],teal:['#35B0A0','#15524B']};
  function send(o){if(host)host.postMessage(o)}
  function show(name,on){document.querySelectorAll('[data-show="'+name+'"]').forEach(function(el){el.style.display=on?'contents':'none'})}
  function render(){
    var c=phase==='play'?'teal':(phase==='upload'||phase==='verifying')?'blue':'orange';
    face.style.backgroundColor=COLORS[c][0]; btn.style.background=COLORS[c][1];
    show('isUpload',phase==='upload'); show('isVerifying',phase==='verifying'); show('isBuild',phase==='build');
    show('isBuilding',phase==='building'); show('isPlay',phase==='play');
    $('buildLabel').textContent=retry?'Retry Build':'Build Recomp';
    var cap='';
    if(error)cap=error;
    else if(phase==='verifying')cap='Checking your disc…';
    else if(phase==='build')cap='Disc verified · ready to build';
    else if(phase==='building')cap=stage==='extract'?'Extracting your disc…':stage==='link'?'Linking the runtime…':'Recompiling game code…';
    else if(phase==='play')cap='Build complete · ready to play';
    var capP=$('capP');
    var visible=phase!=='upload'||!!error;
    var wasHidden=capP.parentNode.style.display==='none';
    show('showCaption',visible);
    if(wasHidden&&visible){capP.style.animation='none';void capP.offsetWidth;capP.style.animation='';}
    capP.style.background=error?'rgba(130,24,10,.82)':'';
    show('showCheck',!error&&(phase==='build'||phase==='play'));
    $('caption').textContent=cap;
    $('pct').textContent=pct; $('bar').style.width=pct+'%';
    $('hint').textContent=phase==='upload'?'Upload ISO':phase==='build'?(retry?'Retry':'Build'):phase==='play'?'Play':'Please wait';
    btn.setAttribute('aria-label',phase==='upload'?'Upload ISO':phase==='verifying'?'Checking disc':phase==='build'?(retry?'Retry Build':'Build Recomp'):phase==='building'?'Building':'Play');
    $('idle').className=(phase==='upload'||phase==='build'||phase==='play')?'sp-idle':'';
    send({cmd:'state',name:error?(phase==='upload'?'disc-error':'build-error'):phase==='building'?'building-'+stage+(pct>=50?'-half':''):phase});
  }
  function press(){
    if(phase==='upload')send({cmd:'pick'});
    else if(phase==='build')send({cmd:'build'});
    else if(phase==='play')send({cmd:'play'});
  }
  if(host)host.addEventListener('message',function(e){
    var m=e.data;
    if(m.type==='verifying'){phase='verifying';error='';retry=false;}
    else if(m.type==='verified'){phase='build';error='';}
    else if(m.type==='discError'){phase='upload';error=m.reason;}
    else if(m.type==='progress'){phase='building';error='';pct=Math.max(0,Math.min(100,Math.floor(m.pct)));stage=m.stage;}
    else if(m.type==='done'){phase='play';pct=100;error='';retry=false;}
    else if(m.type==='buildError'){phase='build';error=m.reason;retry=true;}
    render();
  });
  btn.addEventListener('click',press);
  document.addEventListener('keydown',function(e){if(e.key==='Enter'||e.key===' '){if(document.activeElement!==btn){e.preventDefault();press();}}});
  // A controller's A button (standard mapping: button 0) presses the button, as Enter does.
  var padDown=true;  // ignore an A already held when the page opens
  function pollPads(){
    var pads=navigator.getGamepads?navigator.getGamepads():[], a=false;
    for(var i=0;i<pads.length;i++){var p=pads[i];if(p&&p.buttons&&p.buttons[0]&&p.buttons[0].pressed)a=true;}
    if(a&&!padDown)press();
    padDown=a;
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
      var key=phase+'|'+error;
      if(key!==last&&(phase==='upload'||phase==='build'||phase==='play')){last=key;setTimeout(press,window.SP_AUTOPRESS);}
    },250);
  }
  render();
})();

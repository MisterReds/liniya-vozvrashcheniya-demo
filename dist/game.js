(() => {
  const canvas = document.getElementById('game');
  const W = canvas.width, H = canvas.height, GROUND = 480;
  const outputCtx = canvas.getContext('2d');
  const pixelCanvas = document.createElement('canvas');
  pixelCanvas.width = 550; pixelCanvas.height = 300;
  const ctx = pixelCanvas.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  const $ = id => document.getElementById(id);
  const ui = {
    phase: $('phase-label'), clock: $('clock-label'), objective: $('objective-text'), progress: $('objective-progress'),
    scrap: $('scrap-count'), fuel: $('fuel-count'), gate: $('gate-meter'), gateValue: $('gate-value'),
    hint: $('interaction-hint'), toast: $('toast'), start: $('start-overlay'), end: $('end-overlay'),
    endTitle: $('end-title'), endCopy: $('end-copy'), endEyebrow: $('end-eyebrow'), latest: $('latest-log')
  };
  const keys = new Set();
  let state, last = 0, toastTimer, spawnTimer = 0;

  function reset() {
    state = {
      running:false, phase:'day', time:0, nightTime:0,
      player:{x:135,hp:100,attack:0,facing:1,walk:0}, scrap:0,fuel:0,gate:100,generator:false,nightStarted:false,
      nodes:[{x:270,type:'scrap',amount:2,used:false,label:'Ящик с ломом'},{x:735,type:'scrap',amount:2,used:false,label:'Разобранный вагон'},{x:865,type:'fuel',amount:2,used:false,label:'Канистры'}],
      barricades:[],enemies:[],survivors:[
        {name:'Марта',role:'scavenger',x:355,target:270,walk:0,work:0},
        {name:'Илья',role:'idle',x:405,target:405,walk:0,work:0},
        {name:'Сева',role:'engineer',x:570,target:570,walk:0,work:0}
      ], crates:[{x:465,used:false}],kills:0,nightDuration:52
    };
    spawnTimer=0; keys.clear();
    document.querySelectorAll('.person-card').forEach((c,i)=>c.classList.toggle('selected',i===0));
    document.querySelector('[data-role="scavenger"] .person-info small').textContent='СБОРЩИК';
    document.querySelector('[data-role="guard"] .person-info small').textContent='БЕЗ НАЗНАЧЕНИЯ';
    ui.phase.textContent='РАЗВЕДКА';ui.clock.textContent='ДЕНЬ 1';ui.objective.textContent='Найди лом и топливо для генератора';ui.progress.style.width='0%';
    $('med-count').textContent='1';$('people-count').textContent='4';ui.latest.querySelector('p').textContent='В эфире только помехи.';ui.latest.querySelector('.log-time').textContent='--:--';
    $('people-note').textContent='Марта идёт к куче лома. Дай ей время поработать — это видно на сцене.';
    ui.start.classList.remove('hidden');ui.end.classList.add('hidden');renderUI();
  }
  function toast(message){ui.toast.textContent=message;ui.toast.classList.add('visible');clearTimeout(toastTimer);toastTimer=setTimeout(()=>ui.toast.classList.remove('visible'),1900)}
  function near(x,range=48){return Math.abs(state.player.x-x)<range}
  function getInteractable(){
    for(const n of state.nodes) if(!n.used&&near(n.x,47)) return {kind:'node',obj:n,text:`E  ·  Взять ${n.type==='scrap'?'лом':'топливо'} (+${n.amount})`};
    if(!state.generator&&near(575,58)) return {kind:'generator',text:state.scrap>=3&&state.fuel>=2?'E  ·  Запустить генератор (3 лома, 2 топлива)':'Генератор · нужно 3 лома и 2 топлива'};
    for(let i=0;i<2;i++) if(!state.barricades[i]&&near(390+i*95,34)) return {kind:'barricade',index:i,text:'E  ·  Укрепить ограждение (2 лома)'};
    return null;
  }
  function interact(){
    if(!state.running)return;
    const a=getInteractable(); if(!a){toast('Подойди ближе к объекту');return;}
    if(a.kind==='node'){a.obj.used=true;if(a.obj.type==='scrap')state.scrap+=a.obj.amount;else state.fuel+=a.obj.amount;toast(`Поднято: ${a.obj.type==='scrap'?'лом':'топливо'} +${a.obj.amount}`);}
    if(a.kind==='generator'){
      if(state.scrap<3||state.fuel<2){toast('Не хватает припасов для ремонта');return;}
      state.scrap-=3;state.fuel-=2;state.generator=true;toast('Генератор заработал. Свет виден далеко.');
      ui.objective.textContent='Подготовь станцию и начни первую ночь';ui.progress.style.width='100%';
      const btn=document.createElement('button');btn.id='night-button';btn.className='night-button';btn.textContent='НАЧАТЬ НОЧЬ →';btn.onclick=startNight;document.querySelector('.game-frame').appendChild(btn);
      setLog('06:42','Генератор запущен. На востоке что-то ответило на шум.');
    }
    if(a.kind==='barricade'){if(state.scrap<2){toast('Нужно 2 лома для укрепления');return;}state.scrap-=2;state.barricades[a.index]=true;toast('Укрепление готово.');}
    renderUI();
  }
  function startNight(){if(!state.generator||state.nightStarted)return;state.nightStarted=true;state.phase='night';state.nightTime=0;document.getElementById('night-button')?.remove();ui.phase.textContent='НОЧНОЙ ДОЗОР';ui.objective.textContent='Удержи станцию до рассвета';ui.progress.style.width='0%';toast('Солнце село. Держи проход!');setLog('20:11','Движение у восточного прохода. Илья зарядил винтовку.');}
  function setLog(time,msg){ui.latest.querySelector('.log-time').textContent=time;ui.latest.querySelector('p').textContent=msg}
  function attack(){if(!state.running||state.phase!=='night')return;state.player.attack=.34;const target=state.enemies.find(e=>Math.abs(e.x-state.player.x)<76&&e.hp>0);if(target){target.hp-=1;target.flash=.13;if(target.hp<=0){state.kills++;toast('Угроза остановлена');}}else toast('Удар в пустоту');}
  function update(dt){
    if(!state.running)return;
    state.time+=dt;
    const player=state.player;let moving=false;
    if(keys.has('a')||keys.has('arrowleft')){player.x-=205*dt;player.facing=-1;moving=true;}
    if(keys.has('d')||keys.has('arrowright')){player.x+=205*dt;player.facing=1;moving=true;}
    player.x=Math.max(62,Math.min(1035,player.x));player.walk=moving?player.walk+dt*11:0;player.attack=Math.max(0,player.attack-dt);
    if(state.phase==='day'){
      state.survivors.forEach(s=>{
        s.target=s.role==='scavenger'?270:s.role==='guard'?440:s.role==='engineer'?575:s.x;
        const dx=s.target-s.x;
        if(Math.abs(dx)>2){s.x+=Math.sign(dx)*Math.min(Math.abs(dx),52*dt);s.walk+=dt*8;s.work=0;}
        else {s.x=s.target;s.walk=0;if(s.role==='scavenger'){s.work+=dt;if(s.work>=6){s.work-=6;state.scrap+=1;toast('Марта собрала лом +1');renderUI();}}}
      });
      ui.clock.textContent=state.generator?'СУМЕРКИ':'ДЕНЬ 1';
    }
    if(state.phase==='night'){
      state.nightTime+=dt;spawnTimer+=dt;
      if(spawnTimer>7&&state.nightTime<42){spawnTimer=0;state.enemies.push({x:1080,hp:2,speed:25+Math.random()*10,flash:0,attack:0,walk:0});}
      for(const e of state.enemies){e.flash=Math.max(0,e.flash-dt);e.attack=Math.max(0,e.attack-dt);e.walk+=dt*7;const guard=state.survivors.some(s=>s.role==='guard');if(e.x>440){e.x-=e.speed*dt;}else if(e.attack<=0){e.attack=guard?2.5:1.8;state.gate-=guard?5:9;toast('Удар по ограждению!');}}
      state.enemies=state.enemies.filter(e=>e.hp>0&&e.x>390);state.gate=Math.max(0,state.gate);
      if(state.gate<=0){finish(false);return}if(state.nightTime>=state.nightDuration){finish(true);return}
      ui.clock.textContent=`${Math.max(0,Math.ceil(state.nightDuration-state.nightTime))} СЕК ДО РАССВЕТА`;
      ui.progress.style.width=`${Math.min(100,state.nightTime/state.nightDuration*100)}%`;
    }
    renderUI();
  }
  function renderUI(){ui.scrap.textContent=state?.scrap??0;ui.fuel.textContent=state?.fuel??0;ui.gate.style.width=`${state?.gate??100}%`;ui.gateValue.textContent=`${Math.ceil(state?.gate??100)}%`;}
  function finish(win){state.running=false;ui.end.classList.remove('hidden');document.getElementById('night-button')?.remove();if(win){ui.endEyebrow.textContent='ПЕРВЫЙ УЗЕЛ ВОССТАНОВЛЕН';ui.endTitle.textContent='Станция снова дышит.';ui.endCopy.textContent=`До рассвета дожили ${state.survivors.length} человека. Генератор работает, и по радио пришёл слабый ответ с востока. Завтра можно будет двигаться дальше.`;setLog('05:58','Рассвет. В эфире — слабый ответ с востока.');}else{ui.endEyebrow.textContent='ОГРАЖДЕНИЕ ПРОРВАНО';ui.endTitle.textContent='Станция погасла.';ui.endCopy.textContent='Ночной дозор не удержал проход. Попробуй назначить Илью охранять ворота и подготовить укрепление.';}}

  // The scene is assembled from repeating ground tiles and individually drawn props.
  // All shapes align to the low resolution buffer; no background image is stretched underneath it.
  function rect(x,y,w,h,color){ctx.fillStyle=color;ctx.fillRect(x,y,w,h)}
  function drawScene(night){
    rect(0,0,W,H,night?'#19262a':'#354449');
    rect(0,0,W,82,night?'#17232a':'#405159');
    // broken skyline, radio masts and a wrecked station shed
    rect(40,113,190,105,'#242d2d');rect(52,99,150,15,'#303a37');rect(68,124,18,80,'#141d1e');rect(100,131,38,25,'#111a1a');rect(158,128,20,76,'#151d1d');
    rect(36,217,199,8,'#78694e');rect(47,222,16,18,'#39413d');rect(203,221,12,23,'#303b39');
    // snapped canopy supports and roof fragments
    for(let x=263;x<780;x+=112){rect(x,179,8,178,'#273331');rect(x+5,178,26,7,'#586052');rect(x+25,185,7,32,'#273331');}
    rect(268,173,160,7,'#536052');rect(438,169,145,7,'#49584f');rect(593,178,157,7,'#526054');rect(725,183,44,7,'#526054');
    // distant poles and wire
    rect(900,98,6,188,'#26312f');rect(891,100,24,5,'#667061');rect(965,134,5,151,'#28332f');rect(957,136,23,4,'#667061');
    rect(903,102,3,2,'#879078');for(let x=916;x<965;x+=4)rect(x,103+Math.floor(Math.sin(x*.05)*3),4,2,'#68736b');
    // ground is a seamless repeating strip of hand-built 32px tiles
    for(let x=0;x<W;x+=32){rect(x,GROUND,32,H-GROUND,'#333a33');rect(x,GROUND,32,6,'#77745b');rect(x+2,GROUND+8,14,5,'#45483b');rect(x+18,GROUND+17,11,4,'#41483d');rect(x+7,GROUND+31,5,4,'#858064');rect(x+25,GROUND+38,4,3,'#252d2a');}
    // old rail tracks cross behind the actors
    rect(0,GROUND+48,W,6,'#292d2b');rect(0,GROUND+65,W,6,'#292d2b');
    for(let x=12;x<W;x+=36){rect(x,GROUND+43,5,34,'#605944');}
    // broken sleepers / grass pixels
    for(let x=30;x<W;x+=83){rect(x,GROUND-4,4,11,'#777253');rect(x+8,GROUND-8,3,5,'#849066');}
    if(night){rect(0,0,W,H,'rgba(5,10,17,.43)');if(state.generator){rect(440,328,225,152,'rgba(244,175,82,.13)');rect(500,355,125,95,'rgba(244,175,82,.14)');}}
  }
  function drawGenerator(){rect(551,377,55,75,'#19211f');rect(555,373,48,8,'#69736a');rect(558,385,42,57,'#39433e');rect(572,390,15,13,state.generator?'#f1bd65':'#9b5742');rect(576,394,7,5,state.generator?'#ffe69c':'#392d2a');rect(562,420,34,16,'#202725');if(state.generator){const blink=Math.floor(state.time*4)%2;rect(570,408,5,5,blink?'#ffe99f':'#d38642');}}
  function drawNode(n){if(n.used)return;if(n.type==='scrap'){rect(n.x-19,GROUND-26,39,22,'#46514c');rect(n.x-21,GROUND-32,25,8,'#9b8863');rect(n.x+1,GROUND-37,18,9,'#756b50');rect(n.x-12,GROUND-20,4,10,'#282f2c');rect(n.x+8,GROUND-29,4,10,'#282f2c');rect(n.x-5,GROUND-36,6,4,'#b4a274');}else{rect(n.x-10,GROUND-34,20,34,'#744c2c');rect(n.x-6,GROUND-29,12,20,'#bd8a43');rect(n.x-5,GROUND-40,10,6,'#303a36');}}
  function drawBarricade(i){const x=390+i*95;rect(x,439,46,40,'#6d604c');rect(x-2,438,50,6,'#a49168');rect(x+7,449,4,25,'#302f29');rect(x+30,448,4,27,'#302f29');rect(x+14,443,5,5,'#c0a77a');}
  function drawHuman(x,y,type,name,walk=0,facing=1,attack=0,work=0){
    const phase=Math.floor(walk)%2, step=Math.sin(walk)*3;
    const coat=type==='scavenger'?'#6c7052':type==='engineer'?'#526560':type==='guard'?'#53615c':'#77654c';
    // shadow, legs, coat, head, scarf and tiny moving arm pixels
    rect(x-13,y-3,26,4,'#202725');
    rect(x-8,y-19+(phase?Math.round(step):0),6,19,'#303a37');rect(x+2,y-19-(phase?Math.round(step):0),6,19,'#293230');
    rect(x-11,y-48,22,29,coat);rect(x-13,y-43,5,17,'#353f3b');rect(x+8,y-43,5,17,'#353f3b');
    rect(x-8,y-62,16,15,'#b49a75');rect(x-10,y-65,19,7,'#392f29');rect(x-9,y-50,18,4,'#8d6d52');
    const digging=type==='scavenger'&&walk===0;
    const armY=attack>0?y-45:digging?y-28-Math.round((Math.sin(work*5)+1)*5):y-34+Math.round(Math.sin(walk+1)*2);
    rect(x+(facing>0?8:-12),armY,5,digging?11:16,'#b49a75');
    if(digging){rect(x+13,y-19,3,13,'#9a845a');rect(x+10,y-20,9,3,'#b59a6b');}
    if(type==='guard'||name==='Илья'){rect(x+facing*8,y-39,18*facing,4,'#303b3b');rect(x+facing*22,y-41,5,6,'#aab09a');}
    if(type==='engineer'){rect(x-7,y-39,14,4,'#c39a58');rect(x+9,y-32,4,11,'#a77d3f');}
    if(name==='hero'){rect(x-12,y-68,24,5,'#354941');rect(x+2,y-65,7,4,'#d2b26e');}
  }
  function drawEnemy(e){const x=e.x,y=GROUND,step=Math.sin(e.walk)*3;const fur=e.flash>0?'#f2d6a0':'#40362f';rect(x-20,y-3,40,4,'#20201d');rect(x-13,y-18+step,10,18,'#292825');rect(x+4,y-18-step,10,18,'#292825');rect(x-19,y-39,37,24,fur);rect(x-22,y-49,27,17,fur);rect(x-20,y-55,7,12,'#4d4035');rect(x-4,y-43,5,4,'#e79d52');rect(x+12,y-31,10,5,fur);}
  function draw(){
    ctx.setTransform(.5,0,0,.5,0,0);ctx.clearRect(0,0,W,H);
    const night=state?.phase==='night'&&state?.nightStarted;drawScene(night);
    // lights and fixed station objects
    rect(412,401,15,78,'#252b28');rect(522,401,15,78,'#252b28');rect(410,399,130,7,'#829087');rect(410,407,130,5,'#59685f');for(let x=420;x<536;x+=18)rect(x,407,4,68,'#39443e');
    state?.barricades.forEach((b,i)=>b&&drawBarricade(i));drawGenerator();state?.nodes.forEach(drawNode);
    state?.survivors.forEach(s=>drawHuman(s.x,GROUND,s.role,s.name,s.walk,s.x<=(s.target||s.x)?1:-1,0,s.work));
    if(state)drawHuman(state.player.x,GROUND,'scout','hero',state.player.walk,state.player.facing,state.player.attack);
    state?.enemies.forEach(drawEnemy);
    ctx.fillStyle='#e0bf80';ctx.font='bold 12px monospace';ctx.fillText('ВОСТОК  →',970,447);
    if(state&&state.player.hp<100){rect(state.player.x-20,GROUND-91,40,5,'#372e2b');rect(state.player.x-20,GROUND-91,40*state.player.hp/100,5,'#c56d58');}
    ctx.setTransform(1,0,0,1,0,0);outputCtx.imageSmoothingEnabled=false;outputCtx.clearRect(0,0,W,H);outputCtx.drawImage(pixelCanvas,0,0,W,H);
  }
  function frame(t){const dt=Math.min(.05,(t-last)/1000||0);last=t;update(dt);draw();requestAnimationFrame(frame)}

  $('start-button').addEventListener('click',()=>{ui.start.classList.add('hidden');state.running=true;toast('Подойди к припасам. Кнопками на экране можно идти и действовать.');});
  $('restart-button').addEventListener('click',()=>{reset();state.running=true;});
  $('help-button').addEventListener('click',()=>$('help-modal').classList.remove('hidden'));
  $('close-help').addEventListener('click',()=>$('help-modal').classList.add('hidden'));$('help-ok').addEventListener('click',()=>$('help-modal').classList.add('hidden'));
  document.querySelectorAll('.person-card').forEach(card=>card.addEventListener('click',()=>{
    if(!state)return;const role=card.dataset.role;const survivor=state.survivors.find(s=>s.name===(role==='scavenger'?'Марта':role==='guard'?'Илья':'Сева'));if(!survivor)return;
    state.survivors.forEach(s=>{if(s.role===role)s.role='idle'});survivor.role=role;
    document.querySelectorAll('.person-card').forEach(c=>c.classList.toggle('selected',c===card));card.querySelector('.person-info small').textContent=role==='scavenger'?'СБОРЩИК':role==='guard'?'ОХРАНА':'ИНЖЕНЕР';
    if(role==='guard'){document.querySelector('[data-role="scavenger"] .person-info small').textContent='БЕЗ НАЗНАЧЕНИЯ';toast('Илья идёт к воротам.');$('people-note').textContent='Илья занимает пост у ворот; во время атаки он замедлит потерю прочности.';}
    if(role==='scavenger'){document.querySelector('[data-role="guard"] .person-info small').textContent='БЕЗ НАЗНАЧЕНИЯ';toast('Марта идёт к куче лома.');$('people-note').textContent='Когда Марта дойдёт до лома, она будет собирать по одной детали за раз.';}
    if(role==='engineer')toast('Сева идёт к генератору.');
  }));
  window.addEventListener('keydown',e=>{const k=e.key.toLowerCase();if(['arrowleft','arrowright',' '].includes(k))e.preventDefault();keys.add(k);if(k==='e')interact();if(k===' ')attack();if(k==='escape')$('help-modal').classList.add('hidden')});
  window.addEventListener('keyup',e=>keys.delete(e.key.toLowerCase()));window.addEventListener('blur',()=>keys.clear());
  document.querySelectorAll('[data-hold]').forEach(button=>{
    const key=button.dataset.hold;
    const release=e=>{keys.delete(key);button.classList.remove('pressed');if(e&&button.hasPointerCapture?.(e.pointerId))button.releasePointerCapture(e.pointerId);};
    button.addEventListener('pointerdown',e=>{e.preventDefault();if(!state.running)return;keys.add(key);button.classList.add('pressed');button.setPointerCapture?.(e.pointerId);});
    button.addEventListener('pointerup',release);button.addEventListener('pointercancel',release);button.addEventListener('lostpointercapture',()=>{keys.delete(key);button.classList.remove('pressed')});
  });
  document.querySelector('[data-tap="interact"]').addEventListener('pointerdown',e=>{e.preventDefault();interact()});
  document.querySelector('[data-tap="attack"]').addEventListener('pointerdown',e=>{e.preventDefault();attack()});
  function hintLoop(){if(state&&state.running){const a=getInteractable();ui.hint.textContent=a?a.text.replace(/^E\s+·\s+/,'ДЕЙСТВИЕ · '):'';ui.hint.classList.toggle('visible',!!a);if(!state.generator){const enough=state.scrap>=3&&state.fuel>=2;ui.objective.textContent=enough?'Вернись к генератору и запусти его':`Найди лом (${state.scrap}/3) и топливо (${state.fuel}/2)`;ui.progress.style.width=`${Math.min(100,(state.scrap/3+state.fuel/2)/2*100)}%`;}}requestAnimationFrame(hintLoop)}
  reset();requestAnimationFrame(frame);requestAnimationFrame(hintLoop);
})();

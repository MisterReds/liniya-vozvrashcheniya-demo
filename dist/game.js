(() => {
  const canvas = document.getElementById('game');
  const W = canvas.width, H = canvas.height, GROUND = 480;
  const outputCtx = canvas.getContext('2d');
  const pixelCanvas = document.createElement('canvas');
  pixelCanvas.width = W / 2; pixelCanvas.height = H / 2;
  const ctx = pixelCanvas.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  const stationArt = new Image(); stationArt.src = 'assets/station.png';
  const characterArt = new Image(); characterArt.src = 'assets/characters.png';
  const $ = id => document.getElementById(id);
  const ui = {
    phase: $('phase-label'), clock: $('clock-label'), objective: $('objective-text'), progress: $('objective-progress'),
    scrap: $('scrap-count'), fuel: $('fuel-count'), gate: $('gate-meter'), gateValue: $('gate-value'),
    hint: $('interaction-hint'), toast: $('toast'), start: $('start-overlay'), end: $('end-overlay'),
    endTitle: $('end-title'), endCopy: $('end-copy'), endEyebrow: $('end-eyebrow'), latest: $('latest-log')
  };
  const keys = new Set();
  let state, last = 0, toastTimer, attackTimer = 0, spawnTimer = 0;

  function reset() {
    state = {
      running:false, phase:'day', time:0, nightTime:0, player:{x:135,hp:100,attack:0,facing:1},
      scrap:0,fuel:0,med:1,gate:100,generator:false,nightStarted:false,won:false,lost:false,
      nodes:[{x:270,type:'scrap',amount:2,used:false,label:'Ящик с ломом'},{x:735,type:'scrap',amount:2,used:false,label:'Разобранный вагон'},{x:865,type:'fuel',amount:2,used:false,label:'Канистры'}],
      barricades:[],enemies:[],survivors:[{name:'Марта',role:'scavenger',x:355},{name:'Илья',role:'idle',x:405},{name:'Сева',role:'engineer',x:570}],
      crates:[{x:465,used:false}],logSent:false,kills:0,nightDuration:52
    };
    attackTimer=0;spawnTimer=0;
    document.querySelectorAll('.person-card').forEach((c,i)=>c.classList.toggle('selected',i===0));
    document.querySelector('[data-role="scavenger"] .person-info small').textContent='СБОРЩИК';
    document.querySelector('[data-role="guard"] .person-info small').textContent='БЕЗ НАЗНАЧЕНИЯ';
    ui.phase.textContent='РАЗВЕДКА';ui.clock.textContent='ДЕНЬ 1';ui.objective.textContent='Найди лом и топливо для генератора';ui.progress.style.width='0%';
    $('med-count').textContent='1'; $('people-count').textContent='4'; ui.latest.querySelector('p').textContent='В эфире только помехи.'; ui.latest.querySelector('.log-time').textContent='--:--';
    $('people-note').textContent='Марта будет понемногу находить лом, пока работает станция.';
    ui.start.classList.remove('hidden');ui.end.classList.add('hidden');
    renderUI();
  }
  function toast(message){ui.toast.textContent=message;ui.toast.classList.add('visible');clearTimeout(toastTimer);toastTimer=setTimeout(()=>ui.toast.classList.remove('visible'),1900)}
  function near(x,range=48){return Math.abs(state.player.x-x)<range}
  function getInteractable(){
    for(const n of state.nodes) if(!n.used&&near(n.x,47)) return {kind:'node',obj:n,text:`E  ·  Взять ${n.type==='scrap'?'лом':'топливо'} (+${n.amount})`};
    if(!state.generator&&near(575,58)) return {kind:'generator',text:state.scrap>=3&&state.fuel>=2?'E  ·  Запустить генератор (3 лома, 2 топлива)':`Генератор · нужно 3 лома и 2 топлива`};
    for(let i=0;i<2;i++) if(!state.barricades[i]&&near(390+i*95,34)) return {kind:'barricade',index:i,text:'E  ·  Укрепить ограждение (2 лома)'};
    return null;
  }
  function interact(){
    if(!state.running)return;
    const a=getInteractable(); if(!a){toast('Здесь сейчас нечего делать');return;}
    if(a.kind==='node'){a.obj.used=true;if(a.obj.type==='scrap')state.scrap+=a.obj.amount;else state.fuel+=a.obj.amount;toast(`Найдено: ${a.obj.type==='scrap'?'лом':'топливо'} +${a.obj.amount}`);}
    if(a.kind==='generator'){
      if(state.scrap<3||state.fuel<2){toast('Не хватает припасов для ремонта');return;}
      state.scrap-=3;state.fuel-=2;state.generator=true;toast('Генератор заработал. Свет виден далеко.');
      ui.objective.textContent='Подготовь станцию и начни первую ночь';ui.progress.style.width='100%';
      setTimeout(()=>{if(state.running&&!state.nightStarted){const b=document.getElementById('night-button');if(!b){const btn=document.createElement('button');btn.id='night-button';btn.className='night-button';btn.textContent='НАЧАТЬ НОЧЬ →';btn.onclick=startNight;document.querySelector('.game-frame').appendChild(btn);}}},10);
      setLog('06:42','Генератор запущен. На востоке что-то ответило на шум.');
    }
    if(a.kind==='barricade'){
      if(state.scrap<2){toast('Нужно 2 лома для укрепления');return;}
      state.scrap-=2;state.barricades[a.index]=true;toast('Укрепление готово. Ограждение стало прочнее.');
    }
    renderUI();
  }
  function startNight(){if(!state.generator||state.nightStarted)return;state.nightStarted=true;state.phase='night';state.nightTime=0;document.getElementById('night-button')?.remove();ui.phase.textContent='НОЧНОЙ ДОЗОР';ui.objective.textContent='Удержи станцию до рассвета';ui.progress.style.width='0%';toast('Солнце село. Держи проход!');setLog('20:11','Движение у восточного прохода. Илья зарядил винтовку.');}
  function setLog(time,msg){ui.latest.querySelector('.log-time').textContent=time;ui.latest.querySelector('p').textContent=msg}
  function attack(){if(!state.running||state.phase!=='night')return;state.player.attack=.28;state.player.facing=1;const target=state.enemies.find(e=>Math.abs(e.x-state.player.x)<72&&e.hp>0);if(target){target.hp-=1;target.flash=.13;if(target.hp<=0){state.kills++;toast('Угроза остановлена');}}else toast('Удар в пустоту');}
  function update(dt){
    if(!state.running)return;
    state.time+=dt;
    if(state.player.attack>0)state.player.attack-=dt;
    if(keys.has('a')||keys.has('arrowleft')){state.player.x-=205*dt;state.player.facing=-1}
    if(keys.has('d')||keys.has('arrowright')){state.player.x+=205*dt;state.player.facing=1}
    state.player.x=Math.max(62,Math.min(1035,state.player.x));
    if(state.phase==='day'){
      state.survivors.filter(s=>s.role==='scavenger').forEach(s=>{s.timer=(s.timer||0)+dt;if(s.timer>14){s.timer=0;state.scrap++;toast('Марта принесла лом +1');}});
      if(state.generator)ui.clock.textContent='СУМЕРКИ';else ui.clock.textContent='ДЕНЬ 1';
    }
    if(state.phase==='night'){
      state.nightTime+=dt;spawnTimer+=dt;
      if(spawnTimer>7&&state.nightTime<42){spawnTimer=0;state.enemies.push({x:1080,y:GROUND-29,hp:2,speed:25+Math.random()*10,flash:0,attack:0});}
      for(const e of state.enemies){
        e.flash=Math.max(0,e.flash-dt);e.attack=Math.max(0,e.attack-dt);
        const guard=state.survivors.some(s=>s.role==='guard');
        if(e.x>440){e.x-=e.speed*dt;} else if(e.attack<=0){e.attack=guard?2.5:1.8;state.gate-=guard?5:9;toast('Удар по ограждению!');}
      }
      state.enemies=state.enemies.filter(e=>e.hp>0&&e.x>390);
      state.gate=Math.max(0,state.gate);
      if(state.gate<=0){finish(false);return}
      if(state.nightTime>=state.nightDuration){finish(true);return}
      ui.clock.textContent=`${Math.max(0,Math.ceil(state.nightDuration-state.nightTime))} СЕК ДО РАССВЕТА`;
      ui.progress.style.width=`${Math.min(100,state.nightTime/state.nightDuration*100)}%`;
    }
    renderUI();
  }
  function renderUI(){ui.scrap.textContent=state?.scrap??0;ui.fuel.textContent=state?.fuel??0;ui.gate.style.width=`${state?.gate??100}%`;ui.gateValue.textContent=`${Math.ceil(state?.gate??100)}%`;}
  function finish(win){state.running=false;state.won=win;ui.end.classList.remove('hidden');document.getElementById('night-button')?.remove();if(win){ui.endEyebrow.textContent='ПЕРВЫЙ УЗЕЛ ВОССТАНОВЛЕН';ui.endTitle.textContent='Станция снова дышит.';ui.endCopy.textContent=`До рассвета дожили ${state.survivors.length} человека. Генератор работает, и по радио пришёл слабый ответ с востока. Завтра можно будет двигаться дальше.`;setLog('05:58','Рассвет. В эфире — слабый ответ с востока.');}else{ui.endEyebrow.textContent='ОГРАЖДЕНИЕ ПРОРВАНО';ui.endTitle.textContent='Станция погасла.';ui.endCopy.textContent='Ночной дозор не удержал проход. Попробуй распределить людей и лом иначе, прежде чем включать генератор.';}}

  function draw(){
    const night=state?.phase==='night'&&state?.nightStarted;
    ctx.setTransform(.5,0,0,.5,0,0);
    ctx.clearRect(0,0,W,H);
    if(stationArt.complete&&stationArt.naturalWidth){ctx.drawImage(stationArt,0,0,W,H);}
    else {ctx.fillStyle='#263438';ctx.fillRect(0,0,W,H);}
    if(night){ctx.fillStyle='rgba(8,13,18,.42)';ctx.fillRect(0,0,W,H);}
    // Pixel-block pools of light keep the night readable without smooth gradients.
    if(state?.generator){ctx.fillStyle=night?'rgba(240,167,78,.11)':'rgba(240,167,78,.07)';ctx.fillRect(430,325,300,135);ctx.fillStyle=night?'rgba(240,167,78,.13)':'rgba(240,167,78,.09)';ctx.fillRect(485,350,190,90);}
    // gate
    ctx.fillStyle='#252b28';ctx.fillRect(412,401,15,78);ctx.fillRect(522,401,15,78);ctx.fillStyle='#829087';ctx.fillRect(410,399,130,7);ctx.fillStyle='#59685f';for(let x=420;x<536;x+=18)ctx.fillRect(x,407,4,68);
    state?.barricades.forEach((b,i)=>{if(b){const x=390+i*95;ctx.fillStyle='#6d604c';ctx.fillRect(x,439,46,40);ctx.fillStyle='#a49168';ctx.fillRect(x-2,438,50,6);ctx.fillStyle='#302f29';ctx.fillRect(x+7,449,4,25);ctx.fillRect(x+30,448,4,27);}});
    // generator
    ctx.fillStyle='#171e1d';ctx.fillRect(551,377,55,75);ctx.fillStyle='#52605a';ctx.fillRect(555,373,48,8);ctx.fillStyle='#384440';ctx.fillRect(558,385,42,57);ctx.fillStyle=state?.generator?'#f1bd65':'#9b5742';ctx.fillRect(572,390,15,13);ctx.fillStyle=state?.generator?'#ffe69c':'#392d2a';ctx.fillRect(576,394,7,5);ctx.fillStyle='#202725';ctx.fillRect(562,420,34,16);
    // piles / fuel
    state?.nodes.forEach(n=>{if(n.used)return;if(n.type==='scrap'){ctx.fillStyle='#46514c';ctx.fillRect(n.x-19,GROUND-26,39,22);ctx.fillStyle='#9b8863';ctx.fillRect(n.x-21,GROUND-32,25,8);ctx.fillRect(n.x+1,GROUND-37,18,9);ctx.fillStyle='#282f2c';ctx.fillRect(n.x-12,GROUND-20,4,10);ctx.fillRect(n.x+8,GROUND-29,4,10);}else{ctx.fillStyle='#744c2c';ctx.fillRect(n.x-10,GROUND-34,20,34);ctx.fillStyle='#bd8a43';ctx.fillRect(n.x-6,GROUND-29,12,20);ctx.fillStyle='#303a36';ctx.fillRect(n.x-5,GROUND-40,10,6);}});
    // player and villagers
    state?.survivors.forEach(s=>drawHuman(s.x,GROUND,s.role,s.name));
    if(state)drawHuman(state.player.x,GROUND,'scout','hero');
    // enemies
    state?.enemies.forEach(e=>{if(characterArt.complete&&characterArt.naturalWidth){ctx.drawImage(characterArt,630,815,660,385,e.x-41,GROUND-52,82,48);}else{ctx.fillStyle='#332d29';ctx.fillRect(e.x-20,GROUND-31,40,24);ctx.fillStyle='#d19a53';ctx.fillRect(e.x+7,GROUND-27,5,4);}});
    // movement markers / labels
    ctx.fillStyle='#e0bf80';ctx.font='bold 12px monospace';ctx.fillText('ВОСТОК  →',970,447);
    // hit and character health strip
    if(state&&state.player.hp<100){ctx.fillStyle='#372e2b';ctx.fillRect(state.player.x-20,GROUND-91,40,5);ctx.fillStyle='#c56d58';ctx.fillRect(state.player.x-20,GROUND-91,40*state.player.hp/100,5);}
    ctx.setTransform(1,0,0,1,0,0);
    outputCtx.imageSmoothingEnabled=false;outputCtx.clearRect(0,0,W,H);outputCtx.drawImage(pixelCanvas,0,0,W,H);
  }
  function drawHuman(x,y,type,name){
    if(characterArt.complete&&characterArt.naturalWidth){
      let sx=0,sy=0,sw=650,sh=604,dw=56,dh=82;
      if(type==='guard'||name==='Илья'){sx=742;sy=40;sw=555;sh=570;dw=68;dh=82;}
      else if(type==='engineer'||name==='Сева'||name==='Марта'){sx=115;sy=635;sw=390;sh=560;dw=58;dh=82;}
      ctx.drawImage(characterArt,sx,sy,sw,sh,x-dw/2,y-dh,dw,dh);
      if(type==='guard'){ctx.fillStyle='#d5a865';ctx.fillRect(x-4,y-dh-7,8,4);}
    } else {
      ctx.fillStyle='#242c29';ctx.fillRect(x-10,y-42,20,42);ctx.fillStyle='#c5ae85';ctx.fillRect(x-7,y-54,14,13);ctx.fillStyle='#45564f';ctx.fillRect(x-8,y-28,16,7);
    }
  }
  function frame(t){const dt=Math.min(.05,(t-last)/1000||0);last=t;update(dt);draw();requestAnimationFrame(frame)}

  document.getElementById('start-button').addEventListener('click',()=>{ui.start.classList.add('hidden');state.running=true;toast('Осмотрись. Припасы отмечены вдоль путей.');});
  document.getElementById('restart-button').addEventListener('click',()=>{reset();state.running=true;});
  document.getElementById('help-button').addEventListener('click',()=>$('help-modal').classList.remove('hidden'));
  document.getElementById('close-help').addEventListener('click',()=>$('help-modal').classList.add('hidden'));
  document.getElementById('help-ok').addEventListener('click',()=>$('help-modal').classList.add('hidden'));
  document.querySelectorAll('.person-card').forEach(card=>card.addEventListener('click',()=>{
    if(!state)return;const role=card.dataset.role;const survivor=state.survivors.find(s=>s.name===(role==='scavenger'?'Марта':role==='guard'?'Илья':'Сева'));if(!survivor)return;
    state.survivors.forEach(s=>{if(s.role===role)s.role='idle'});survivor.role=role;
    document.querySelectorAll('.person-card').forEach(c=>c.classList.toggle('selected',c===card));
    card.querySelector('.person-info small').textContent=role==='scavenger'?'СБОРЩИК':role==='guard'?'ОХРАНА':'ИНЖЕНЕР';
    if(role==='guard'){document.querySelector('[data-role="scavenger"] .person-info small').textContent='БЕЗ НАЗНАЧЕНИЯ';toast('Илья займёт позицию у прохода.');$('people-note').textContent='Илья будет прикрывать проход во время ночной атаки.';}
    if(role==='scavenger'){document.querySelector('[data-role="guard"] .person-info small').textContent='БЕЗ НАЗНАЧЕНИЯ';toast('Марта отправится собирать лом.');$('people-note').textContent='Марта понемногу находит лом, пока работает станция.';}
    if(role==='engineer')toast('Сева отвечает за генератор.');
  }));
  window.addEventListener('keydown',e=>{const k=e.key.toLowerCase();keys.add(k);if(k==='e'){e.preventDefault();interact()}if(k===' '){e.preventDefault();attack()}if(k==='escape')$('help-modal').classList.add('hidden')});
  window.addEventListener('keyup',e=>keys.delete(e.key.toLowerCase()));
  window.addEventListener('blur',()=>keys.clear());
  function hintLoop(){if(state&&state.running){const a=getInteractable();ui.hint.textContent=a?a.text:'';ui.hint.classList.toggle('visible',!!a);if(!state.generator){const enough=state.scrap>=3&&state.fuel>=2;ui.objective.textContent=enough?'Вернись к генератору и запусти его':`Найди лом (${state.scrap}/3) и топливо (${state.fuel}/2)`;ui.progress.style.width=`${Math.min(100,(state.scrap/3+state.fuel/2)/2*100)}%`;}}requestAnimationFrame(hintLoop)}
  reset();requestAnimationFrame(frame);requestAnimationFrame(hintLoop);
})();

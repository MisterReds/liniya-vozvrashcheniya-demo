(() => {
  const canvas = document.getElementById('game');
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height, GROUND = 480;
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
    ctx.clearRect(0,0,W,H);
    // Sky and distant industrial silhouettes.
    const sky=ctx.createLinearGradient(0,0,0,H);sky.addColorStop(0,night?'#10191f':'#475452');sky.addColorStop(.62,night?'#222d30':'#707366');sky.addColorStop(1,'#464d46');ctx.fillStyle=sky;ctx.fillRect(0,0,W,H);
    if(!night){ctx.fillStyle='#d0a66d';ctx.globalAlpha=.24;ctx.beginPath();ctx.arc(846,147,36,0,Math.PI*2);ctx.fill();ctx.globalAlpha=1;}
    // distant skyline
    ctx.fillStyle=night?'#182326':'#394442';
    const blocks=[[0,292,145,147],[118,270,100,169],[214,311,145,128],[359,280,88,159],[447,301,119,138],[566,263,105,176],[670,307,117,132],[787,284,92,155],[879,300,124,139],[1000,269,100,170]];
    blocks.forEach(([x,y,w,h],i)=>{ctx.fillRect(x,y,w,h);ctx.fillStyle=night?'#202d30':'#434c47';for(let j=0;j<4;j++){ctx.fillRect(x+12+j*22,y+22+(i%3)*3,7,18);}ctx.fillStyle=night?'#182326':'#394442';});
    // broken rail and platform
    ctx.fillStyle='#51564c';ctx.fillRect(0,422,W,58);ctx.fillStyle='#777363';ctx.fillRect(0,421,W,3);
    ctx.fillStyle='#2d3534';for(let x=0;x<W;x+=50){ctx.fillRect(x,476,30,8);}ctx.fillStyle='#858073';ctx.fillRect(0,489,W,4);ctx.fillRect(0,510,W,3);
    // station ruins
    ctx.fillStyle='#323c3c';ctx.fillRect(70,300,470,124);ctx.fillStyle='#65706a';ctx.fillRect(67,294,477,9);ctx.fillStyle='#252e30';ctx.fillRect(83,317,103,84);ctx.fillRect(205,317,126,84);ctx.fillRect(349,317,80,84);ctx.fillRect(449,317,74,84);
    ctx.fillStyle='#5d675f';ctx.fillRect(95,325,78,70);ctx.fillRect(217,325,102,70);ctx.fillRect(360,325,57,70);ctx.fillRect(460,325,51,70);
    ctx.fillStyle=night?'#141b1d':'#4e5851';ctx.fillRect(101,331,66,58);ctx.fillRect(223,331,90,58);ctx.fillRect(366,331,45,58);ctx.fillRect(466,331,39,58);
    // hanging sign
    ctx.fillStyle='#20292b';ctx.fillRect(183,274,142,32);ctx.fillStyle='#b1a278';ctx.font='10px monospace';ctx.fillText('СТАНЦИЯ  ·  0 КМ',196,294);
    // gate
    ctx.fillStyle='#394340';ctx.fillRect(412,401,15,78);ctx.fillRect(522,401,15,78);ctx.fillStyle='#6b7468';ctx.fillRect(410,399,130,7);ctx.fillStyle='#52635c';for(let x=420;x<536;x+=18)ctx.fillRect(x,407,4,68);
    state?.barricades.forEach((b,i)=>{if(b){ctx.fillStyle='#786e56';ctx.fillRect(392+i*95,443,42,34);ctx.fillStyle='#a09473';ctx.fillRect(390+i*95,442,46,5);}});
    // generator
    ctx.fillStyle='#303b3d';ctx.fillRect(554,370,49,77);ctx.fillStyle='#64716b';ctx.fillRect(560,376,36,64);ctx.fillStyle=state?.generator?(night?'#d0a264':'#d5b56e'):'#7a5546';ctx.beginPath();ctx.arc(578,392,7,0,Math.PI*2);ctx.fill();ctx.fillStyle='#20292a';ctx.fillRect(563,409,31,20);
    if(state?.generator){ctx.save();ctx.globalAlpha=night ? 0.2 : 0.12;const glow=ctx.createRadialGradient(579,396,8,579,396,240);glow.addColorStop(0,'#f5bd70');glow.addColorStop(1,'transparent');ctx.fillStyle=glow;ctx.fillRect(330,150,500,390);ctx.restore();}
    // piles / fuel
    state?.nodes.forEach(n=>{if(n.used)return;if(n.type==='scrap'){ctx.fillStyle='#777467';ctx.fillRect(n.x-19,GROUND-27,39,24);ctx.fillStyle='#a69b7e';ctx.fillRect(n.x-16,GROUND-31,23,8);ctx.fillStyle='#444b48';ctx.fillRect(n.x+5,GROUND-38,17,10);}else{ctx.fillStyle='#9b6f42';ctx.fillRect(n.x-9,GROUND-34,17,34);ctx.fillStyle='#c9a35f';ctx.fillRect(n.x-6,GROUND-30,11,20);ctx.fillStyle='#565c53';ctx.fillRect(n.x-5,GROUND-40,9,6);}});
    // player and villagers
    state?.survivors.forEach((s,i)=>drawHuman(s.x,GROUND-28,['#b58e62','#77908a','#8c708b'][i],s.role==='guard'));
    if(state)drawHuman(state.player.x,GROUND-35,'#d1d0b9',false,true);
    // enemies
    state?.enemies.forEach(e=>{ctx.save();ctx.translate(e.x,GROUND-24);ctx.fillStyle=e.flash?'#e9d5aa':'#191c1a';ctx.beginPath();ctx.ellipse(0,0,21,15,0,0,Math.PI*2);ctx.fill();ctx.fillRect(-14,-23,27,14);ctx.fillStyle='#bd6550';ctx.fillRect(6,-18,4,3);ctx.fillStyle='#343831';ctx.fillRect(-13,9,5,17);ctx.fillRect(7,9,5,17);ctx.restore();});
    // movement markers / labels
    ctx.fillStyle='#87918a';ctx.font='9px monospace';ctx.fillText('К ВОСТОКУ  →',965,458);
    if(night){ctx.fillStyle='#070d10';ctx.globalAlpha=.25;ctx.fillRect(0,0,W,H);ctx.globalAlpha=1;if(state.generator){const g=ctx.createRadialGradient(579,400,25,579,400,330);g.addColorStop(0,'#d1a46630');g.addColorStop(1,'#081013cc');ctx.fillStyle=g;ctx.fillRect(240,40,680,540);}}
    // night sky motes
    if(night){for(let i=0;i<24;i++){ctx.fillStyle=`rgba(205,220,204,${.16+(i%3)*.08})`;ctx.fillRect((i*97+31)%W,60+(i*53)%240,1,1);}}
    // hit and character health strip
    if(state&&state.player.hp<100){ctx.fillStyle='#372e2b';ctx.fillRect(state.player.x-17,GROUND-73,34,3);ctx.fillStyle='#c56d58';ctx.fillRect(state.player.x-17,GROUND-73,34*state.player.hp/100,3);}
  }
  function drawHuman(x,y,color,guard,hero=false){ctx.save();ctx.translate(x,y);ctx.fillStyle='#171b1a';ctx.fillRect(-10,24,20,4);ctx.fillStyle=color;ctx.fillRect(-7,0,14,22);ctx.fillStyle='#c9bca0';ctx.beginPath();ctx.arc(0,-5,7,0,Math.PI*2);ctx.fill();ctx.fillStyle='#272e2d';ctx.fillRect(-6,16,5,12);ctx.fillRect(2,16,5,12);if(guard){ctx.strokeStyle='#8c9a8a';ctx.lineWidth=3;ctx.beginPath();ctx.moveTo(8,5);ctx.lineTo(18,-2);ctx.stroke();}if(hero){ctx.fillStyle='#d0a769';ctx.fillRect(-8,-16,16,3);}ctx.restore();}
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

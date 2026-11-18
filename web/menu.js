const picker = document.querySelector('.control-picker');
const schemes = [...picker.querySelectorAll('[data-scheme]')];
const layouts = [...document.querySelectorAll('[data-layout]')];

function selectScheme(name) {
  for (const button of schemes) button.setAttribute('aria-pressed',String(button.dataset.scheme === name));
  for (const list of layouts) list.hidden = list.dataset.layout !== name;
}

picker.addEventListener('click',event=>{
  const button=event.target.closest('[data-scheme]');
  if(button) selectScheme(button.dataset.scheme);
});
picker.addEventListener('keydown',event=>{
  if(event.key!=='ArrowLeft' && event.key!=='ArrowRight')return;
  const current=schemes.indexOf(document.activeElement);
  if(current<0)return;
  event.preventDefault();
  const next=schemes[(current+(event.key==='ArrowRight'?1:-1)+schemes.length)%schemes.length];
  next.focus();selectScheme(next.dataset.scheme);
});
selectScheme(matchMedia('(pointer: coarse)').matches?'touch':'keys');
window.addEventListener('gamepadconnected',()=>selectScheme('pad'));

// Remember cockpit preferences independently of the player's saved ride.
for (const [name, fallback, apply] of [
  ['steer-side','left',value=>document.body.dataset.steerSide=value],
  ['render-quality','balanced',value=>{
    document.body.dataset.renderQuality=value;
    window.dispatchEvent(new Event('circuit-resolution-change'));
  }]
]) {
  const options=[...document.querySelectorAll('input[name="'+name+'"]')];
  const key='neon-ride.touch.'+name;
  let selected=fallback;
  try { selected=localStorage.getItem(key)||fallback; } catch {}
  const current=options.find(option=>option.value===selected)||options[0];
  current.checked=true;
  apply(current.value);
  for (const option of options) option.addEventListener('change',()=>{
    if(!option.checked)return;
    apply(option.value);
    try {localStorage.setItem(key,option.value);} catch {}
  });
}

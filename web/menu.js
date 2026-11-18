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

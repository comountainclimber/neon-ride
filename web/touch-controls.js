// Each finger owns one control. Dragging across the pedal buttons rolls the right
// (or left-handed) thumb from throttle to nitro without lifting it.
export function touchControls(root, isActive) {
  const pad = root.querySelector('[data-steering]');
  const knob = pad.querySelector('.steer-grip');
  const pedals = [...root.querySelectorAll('[data-pedal]')];
  const pointers = new Map();
  const state = {steer: 0, gas: 0, brake: 0, boost: false};
  let steeringPointer = null;
  let keyboardPedal = null;
  let thumbPosition = 0;

  function steer(position) {
    const clamped = Math.max(-1, Math.min(1, position));
    thumbPosition = clamped;
    const travel = Math.max(1, (pad.clientWidth - knob.offsetWidth - 16) / 2);
    const outsideNeutral = Math.max(0, (Math.abs(clamped) - .075) / .925);
    // A soft center gives small course corrections more travel under the thumb.
    state.steer = Math.sign(clamped) * outsideNeutral * (.58 + .42 * outsideNeutral);
    pad.style.setProperty('--thumb-offset', (clamped * travel).toFixed(2) + 'px');
    const percent = Math.round(state.steer * 100);
    pad.setAttribute('aria-valuenow', String(percent));
    pad.setAttribute('aria-valuetext', percent ? Math.abs(percent) + '% ' + (percent < 0 ? 'left' : 'right') : 'Centered');
  }

  function steerAt(x) {
    const rect = pad.getBoundingClientRect();
    const travel = Math.max(1, (rect.width - knob.offsetWidth - 16) / 2);
    steer((x - rect.left - rect.width / 2) / travel);
  }

  function updatePedals() {
    const held = new Set(pointers.values());
    if (keyboardPedal) held.add(keyboardPedal.dataset.pedal);
    state.brake = Number(held.has('brake'));
    state.boost = held.has('boost');
    state.gas = Number(held.has('gas') || state.boost);
    for (const button of pedals) {
      button.setAttribute('aria-pressed', String(held.has(button.dataset.pedal)));
    }
  }

  function end(id) {
    if (!pointers.has(id)) return;
    pointers.delete(id);
    if (steeringPointer === id) {
      steeringPointer = null;
      pad.removeAttribute('data-held');
      steer(0);
    }
    updatePedals();
  }

  function reset() {
    const ids = [...pointers.keys()];
    pointers.clear();
    keyboardPedal = null;
    steeringPointer = null;
    pad.removeAttribute('data-held');
    steer(0);
    updatePedals();
    for (const id of ids) {
      if (root.hasPointerCapture(id)) root.releasePointerCapture(id);
    }
  }

  root.addEventListener('pointerdown', event => {
    if (!isActive() || event.button !== 0) return;
    const target = event.target.closest('[data-steering],[data-pedal]');
    if (!target || (target === pad && steeringPointer !== null)) return;
    event.preventDefault();
    if (target === pad) {
      steeringPointer = event.pointerId;
      pointers.set(event.pointerId, 'steer');
      pad.setAttribute('data-held', '');
      steerAt(event.clientX);
    } else {
      pointers.set(event.pointerId, target.dataset.pedal);
      updatePedals();
    }
    root.setPointerCapture(event.pointerId);
  });

  root.addEventListener('pointermove', event => {
    if (!pointers.has(event.pointerId)) return;
    if (steeringPointer === event.pointerId) {
      steerAt(event.clientX);
      return;
    }
    const button = document.elementFromPoint(event.clientX, event.clientY)?.closest('[data-pedal]');
    if (button && root.contains(button) && button.dataset.pedal !== pointers.get(event.pointerId)) {
      pointers.set(event.pointerId, button.dataset.pedal);
      updatePedals();
    }
  });

  for (const type of ['pointerup','pointercancel','lostpointercapture']) {
    root.addEventListener(type, event => end(event.pointerId));
  }
  root.addEventListener('contextmenu', event => event.preventDefault());

  for (const button of pedals) {
    button.addEventListener('keydown', event => {
      if (!isActive() || ![' ', 'Enter'].includes(event.key)) return;
      event.preventDefault();event.stopPropagation();
      keyboardPedal = button;
      updatePedals();
    });
    const release = () => {
      if (keyboardPedal !== button) return;
      keyboardPedal = null;
      updatePedals();
    };
    button.addEventListener('keyup', event => { if ([' ', 'Enter'].includes(event.key)) release(); });
    button.addEventListener('blur', release);
  }

  pad.addEventListener('keydown', event => {
    if (!isActive() || !['ArrowLeft','ArrowRight','Home','End'].includes(event.key)) return;
    event.preventDefault();
    event.stopPropagation();
    pad.setAttribute('data-held', '');
    steer(event.key === 'Home' ? -1 : event.key === 'End' ? 1 : thumbPosition + (event.key === 'ArrowLeft' ? -.22 : .22));
  });
  pad.addEventListener('keyup', event => {
    if (!['ArrowLeft','ArrowRight','Home','End'].includes(event.key)) return;
    if (steeringPointer === null) { pad.removeAttribute('data-held'); steer(0); }
  });
  pad.addEventListener('blur', () => {
    if (steeringPointer === null) { pad.removeAttribute('data-held'); steer(0); }
  });
  const orientation = matchMedia('(orientation: portrait)');
  orientation.addEventListener('change', reset);
  window.screen.orientation?.addEventListener('change', reset);
  window.addEventListener('pagehide', reset);
  return {state, reset};
}

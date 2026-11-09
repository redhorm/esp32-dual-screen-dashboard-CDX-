/* Browser-only presentation preferences. No API, hardware or application state changes. */
(()=>{
  const button=document.getElementById('themeToggle');
  const query=window.matchMedia('(prefers-color-scheme: light)');
  let selected=null;
  try{selected=localStorage.getItem('storm-theme')}catch(_){}
  function paint(theme){
    const light=theme==='light';document.body.dataset.theme=light?'light':'dark';
    button.setAttribute('aria-label',light?'Attiva tema scuro':'Attiva tema chiaro');
    button.setAttribute('aria-pressed',String(light));
    document.querySelector('meta[name="theme-color"]').setAttribute('content',light?'#eeede7':'#0b0b0b');
  }
  paint(selected==='light'||selected==='dark'?selected:(query.matches?'light':'dark'));
  button.addEventListener('click',()=>{selected=document.body.dataset.theme==='dark'?'light':'dark';paint(selected);try{localStorage.setItem('storm-theme',selected)}catch(_){}});
  if(query.addEventListener)query.addEventListener('change',()=>{if(!selected)paint(query.matches?'light':'dark')});
})();

var e;function t(e,t,n){var l=new SpeechSynthesisUtterance;const a=document.getElementById("voices");l.lang="en",l.text=`${e} lap ${t} ${n} seconds`,console.log(l.voice),l.voice=speechSynthesis.getVoices().filter((function(e){return e.name===a.value}))[0],speechSynthesis.speak(l)}function n(){var n=new XMLHttpRequest;n.open("GET","get_laptimes"),n.onload=function(){if(200===n.status)if(n.responseText){e=JSON.parse(JSON.stringify(n.responseText));var l=JSON.parse(e),a=l.race_num,o=l.race_mode;if(0==a)return;console.log(o),document.getElementById("race_mode").innerHTML=o?"Racing":"Finished";var s=parseInt(l.count_first),c=document.getElementById("lap_table_"+a);if(null==c){(c=document.getElementById("lap_table_default").cloneNode(!0)).id="lap_table_"+a;var i=document.getElementById("table_container"),d=document.createElement("div");d.setAttribute("class","tabcontent"),d.id="round_tab_content_"+a,i.appendChild(d),d.appendChild(c);var r=document.getElementById("round_buttons");console.log(r),r.innerHTML+="<button class='tablinks' onclick='openRound(event,"+a+")'>Round "+a+"</button>\n",r.children[r.childElementCount-1].click(),window.scrollTo(0,document.body.scrollHeight)}l.lap_data;for(var u=c.rows[0].cells.length-3,p=0;p<l.lap_data.length;++p)if(0!=l.lap_data[p].laps.length){var m,_=l.lap_data[p].pilot,g=c.rows[_+1],f=99999,h=0;for(m=0;m<l.lap_data[p].laps.length&&m<u;++m){var v=l.lap_data[p].laps[m];if(0!=s||0!=m)if(f=Math.min(f,v),h+=v,""==g.cells[m+1].innerText)t(g.cells[0].children[0].value,m,(v/1e3).toFixed(2));g.cells[m+1].innerText=v/1e3}h/=m-(0==s),g.cells[u+1].innerText=h/1e3,g.cells[u+2].innerText=f/1e3}}else console.log("Request failed.\tReturned status of "+n.status)},n.send()}n(),setInterval(n,2e3);
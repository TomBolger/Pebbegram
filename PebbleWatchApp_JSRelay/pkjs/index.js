var SERVER = localStorage.getItem('server_url') || 'http://10.0.2.2:8080'; // change to your server

function send(dict) {
  Pebble.sendAppMessage(dict, function(){}, function(e){ console.log('send fail', JSON.stringify(e)); });
}

function streamChats(chats){
  chats.slice(0,10).forEach(function(c, i){
    send({'0':'chat_item','1':i,'2':c.title,'3':c.id});
  });
  send({'0':'chats_done'});
}

function streamMessages(msgs){
  msgs.slice(0,20).forEach(function(m, i){
    send({'0':'message_item','1':i,'2':m.text,'3':m.out ? 1 : 0});
  });
  send({'0':'messages_done'});
}

Pebble.addEventListener('appmessage', function(e) {
  var p = e.payload;
  var type = p['0'];
  if (type === 'get_chats') {
    fetch(SERVER + '/api/chats').then(r=>r.json()).then(streamChats).catch(err=>console.log('chats err', err));
  } else if (type === 'get_messages') {
    var chatId = p['1'];
    fetch(SERVER + '/api/messages?chat_id=' + chatId).then(r=>r.json()).then(streamMessages).catch(err=>console.log('msgs err', err));
  } else if (type === 'send') {
    var chatId = p['1'];
    var text = p['2'];
    fetch(SERVER + '/api/send', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({chat_id: chatId, text: text})})
      .then(r=>r.json()).then(function(){ /* noop */ }).catch(err=>console.log('send err', err));
  }
});

Pebble.addEventListener('showConfiguration', function() {
  var html = '<!doctype html><html><body style="font-family:sans-serif;padding:20px;">' +
    '<h3>Telegram Relay Settings</h3>' +
    '<label>Server URL: <input id=\"u\" style=\"width:100%\" value=\"' + SERVER + '\"></label>' +
    '<button id=\"s\" style=\"margin-top:12px;\">Save</button>' +
    '<script>document.getElementById(\"s\").onclick=function(){' +
    'var u=document.getElementById(\"u\").value;location.href=\"pebblejs://close#\"+encodeURIComponent(JSON.stringify({server_url:u}))};</script>' +
    '</body></html>';
  Pebble.openURL('data:text/html,' + encodeURIComponent(html));
});

Pebble.addEventListener('webviewclosed', function(e) {
  try{
    var resp = JSON.parse(decodeURIComponent(e.response));
    if (resp.server_url) {
      localStorage.setItem('server_url', resp.server_url);
      SERVER = resp.server_url;
    }
  }catch(err){}
});

console.log('PKJS v2 ready');
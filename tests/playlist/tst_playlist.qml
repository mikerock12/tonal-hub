import QtQuick
import QtQuick.Controls
import QtTest

Item {
    id:host;width:1040;height:880
    Rectangle {anchors.fill:parent;color:"#101112"}
    QtObject {
        id:mock
        property var rows:[]
        property bool busy:false
        property bool syncing:false
        property string status:"Cole músicas de um show ou uma lista de faixas."
        property string detectedArtist:""
        property string cover:""
        property string savedId:""
        property int selectedCount:0
        property var details:({})
        property int spotifyWrites:0
        property int retries:0
        property string targetId:""
        function setTargetPlaylist(id){targetId=id;return true}
        function cancel(){}
        // Sections mirror TextPlaylistImport::sectionOf: 25 selected, 4 not found (homonyms only), 1 live alternate.
        function parse(text,format,artist){targetId="";spotifyWrites=0;let generated=[];for(let i=0;i<30;i++){let missing=i%7===3,review=i===14;
            const homonym={title:"Música "+i,artist:"Outra Banda",album:"Álbum homônimo",image:"",match:"WRONG_ARTIST",matchLabel:"Artista errado",reason:"Mesmo título, artista não corresponde (Outra Banda)",requestedArtist:false}
            const own={title:review?"Música "+i+" - Live":"Música "+i,artist:"Banda "+i,album:"Álbum sintético",image:"",match:review?"ALTERNATE_VERSION":"EXACT_MATCH",matchLabel:review?"Versão alternativa":"Encontrada",
                reason:review?"Artista confirmado (ID Spotify); versão ao vivo (pedido: estúdio)":"Artista confirmado (ID Spotify); título idêntico",requestedArtist:true}
            generated.push({requestId:i+1,artist:i===0?"Veruca Salt":"Banda "+i,title:i===0?"Seether":"Música "+i,status:missing?"Não encontrada":review?"Versão alternativa":"Encontrada",
                match:missing?"NOT_FOUND":review?"ALTERNATE_VERSION":"EXACT_MATCH",section:missing?"wrongArtist":review?"alternate":"selected",reason:missing?"Nenhuma gravação com esse título; 1 resultado(s) de outros artistas separados":"",
                choice:missing?-1:0,selected:!missing&&!review,manual:false,ignored:false,candidates:missing?[homonym]:[own,homonym]})}rows=generated;selectedCount=25}
        function search(){status="Revise as gravações antes de salvar."}
        function sectionOf(r){return r.ignored?"ignored":r.selected&&r.choice>=0?"selected":r.match==="ALTERNATE_VERSION"?"alternate":"wrongArtist"}
        function update(row,changes){let updated=rows.slice();updated[row]=Object.assign({},updated[row],changes);updated[row].section=sectionOf(updated[row]);rows=updated;selectedCount=updated.filter(r=>r.selected&&r.choice>=0).length}
        function choose(row,candidate,on){update(row,{choice:candidate,selected:on,manual:true,ignored:!on,status:on?"Escolha manual":"Ignorada"})}
        function ignore(row){update(row,{selected:false,manual:true,ignored:true,status:"Ignorada"})}
        function retry(row){retries++;update(row,{manual:false,ignored:false})}
        function edit(row,artist,title){}
        function move(row,direction){}
        function clearCover(){cover=""}
        function setCover(url){return true}
        function saveLocal(name,description,dedup){savedId="local-fixture";details={name:name,description:description,cover:"",tracks:["uri"],appendMode:targetId.length>0,spotifyId:targetId};status="Playlist salva no Tonalify. Deseja salvar também no Spotify?";return true}
        function sync(pub){spotifyWrites++;status="Playlist salva no Tonalify e no Spotify."}
    }
    TextPlaylistDialog {id:wizard;controller:mock;parent:host}
    TestCase {
        name:"TextPlaylistScreen";when:windowShown
        function test_local_first_native_wizard(){
            wizard.fresh();tryCompare(wizard,"visible",true)
            const input=findChild(host,"playlistTextInput"),next=findChild(host,"playlistContinue")
            verify(input!==null);input.text="Veruca Salt - Seether\nL7 - Shove"
            mouseClick(next);tryCompare(wizard,"step",1);compare(mock.spotifyWrites,0)
            const filter=findChild(host,"missingFilter"),list=findChild(host,"reviewList");verify(filter!==null&&list!==null);tryCompare(list,"count",30);compare(filter.text,"Só pendências (5)")
            mouseClick(filter);tryCompare(list,"count",5);mouseClick(filter);tryCompare(list,"count",30)
            compare(wizard.sectionCount("wrongArtist"),4);compare(wizard.sectionCount("alternate"),1);compare(wizard.sectionCount("selected"),25)
            wizard.filter="wrongArtist";tryCompare(list,"count",4);wizard.filter="all";tryCompare(list,"count",30)
            list.contentY=480;wait(60);const beforeY=list.contentY;wizard.chooseReview(1,0,false);wait(80);compare(list.contentY,beforeY);compare(mock.selectedCount,24)
            compare(wizard.sectionCount("ignored"),1)
            grabImage(host).save("review.png")
            // Homonyms are listed apart in "Escolher gravação" and need confirmation.
            const entries=wizard.versionEntries(mock.rows[2].candidates);compare(entries.length,3);compare(entries[0].requestedArtist,true);verify(entries[1].header!==undefined);compare(entries[2].requestedArtist,false)
            const only=wizard.versionEntries(mock.rows[3].candidates);verify(only[0].header.indexOf("Nenhuma gravação do artista pedido")===0)
            mouseClick(next);tryCompare(wizard,"step",2)
            findChild(host,"importPlaylistName").text="Guitarras de hoje"
            findChild(host,"importPlaylistDescription").text="Uma coleção criada a partir de texto"
            grabImage(host).save("details.png")
            mouseClick(next);tryCompare(wizard,"step",3);compare(mock.details.name,"Guitarras de hoje");compare(mock.spotifyWrites,0)
            grabImage(host).save("local-saved.png")
            mouseClick(findChild(host,"playlistSendSpotify"));compare(mock.spotifyWrites,1)
            wizard.close()
        }
        function test_existing_playlist_destination(){
            wizard.playlistTargets=[{id:"abcdefghijklmnopqrstuv",name:"Playlist de destino"}]
            wizard.fresh();const input=findChild(host,"playlistTextInput"),next=findChild(host,"playlistContinue");input.text="Veruca Salt - Seether"
            mouseClick(next);tryCompare(wizard,"step",1);mouseClick(next);tryCompare(wizard,"step",2)
            const target=findChild(host,"importTargetPlaylist");verify(target!==null);tryCompare(target,"count",2);target.currentIndex=1
            mouseClick(next);tryCompare(wizard,"step",3);compare(mock.targetId,"abcdefghijklmnopqrstuv");compare(mock.details.appendMode,true);compare(mock.spotifyWrites,0)
            compare(findChild(host,"playlistSendSpotify").text,"Adicionar músicas à playlist");mouseClick(findChild(host,"playlistSendSpotify"));compare(mock.spotifyWrites,1)
            wizard.close();wizard.playlistTargets=[]
        }
    }
}

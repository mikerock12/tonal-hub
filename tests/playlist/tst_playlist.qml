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
        property bool dirty:false
        property var drafts:[]
        property var batches:[]
        property var pins:({})
        property int savedDrafts:0
        property int discards:0
        property var summary:({active:rows.length,replacedTotal:0,toSave:selectedCount})
        function refreshDrafts(){}
        function saveDraft(name){savedDrafts++;dirty=false;drafts=[{id:"d1",name:name||"Rascunho de teste",total:rows.length,selected:selectedCount,pending:rows.length-selectedCount,updatedAt:"2026-11-28T10:00:00Z",recovery:false}];status="Rascunho salvo";return true}
        function discardChanges(){discards++;dirty=false;rows=[]}
        function loadDraft(id){parse("",0,"");dirty=false;return true}
        function deleteDraft(id){drafts=[];return true}
        function renameDraft(id,name){return true}
        function previewBatch(text,format,artist){const lines=text.split("\n").filter(l=>l.trim().length);return {count:lines.length,newCount:lines.length,duplicates:[],repeatedList:false,pending:[{requestId:4,artist:"Banda 3",title:"Música 3",section:"wrongArtist"}]}}
        function addBatch(text,format,artist,mode,ids,dup){const lines=text.split("\n").filter(l=>l.trim().length);let updated=rows.slice();for(const l of lines)updated.push({requestId:updated.length+1,artist:l.split(" - ")[0],title:l.split(" - ")[1],status:"Encontrada",match:"EXACT_MATCH",section:"selected",batchId:"b2",choice:0,selected:true,manual:false,ignored:false,candidates:[{title:l.split(" - ")[1],artist:l.split(" - ")[0],album:"Novo",image:"",requestedArtist:true,group:"requested"}]});
            rows=updated;selectedCount=updated.filter(r=>r.selected).length;batches=[{batchId:"b1"},{batchId:"b2"}];dirty=true;lastMode=mode;return true}
        property string lastMode:""
        function undoLastBatch(){rows=rows.filter(r=>r.batchId!=="b2");batches=[{batchId:"b1"}];return true}
        function pinArtist(row,c){pins={"scrawl":{id:"x",name:"Scrawl",album:"Smallmouth"}};return true}
        function setTargetPlaylist(id){targetId=id;return true}
        function cancel(){}
        // Sections mirror TextPlaylistImport::sectionOf: 25 selected, 4 not found (homonyms only), 1 live alternate.
        function parse(text,format,artist){targetId="";spotifyWrites=0;let generated=[];for(let i=0;i<30;i++){let missing=i%7===3,review=i===14;
            const homonym={title:"Música "+i,artist:"Outra Banda",album:"Álbum homônimo",image:"",match:"WRONG_ARTIST",matchLabel:"Artista errado",reason:"Mesmo título, artista não corresponde (Outra Banda)",requestedArtist:false}
            const own={title:review?"Música "+i+" - Live":"Música "+i,artist:"Banda "+i,album:"Álbum sintético",image:"",match:review?"ALTERNATE_VERSION":"EXACT_MATCH",matchLabel:review?"Versão alternativa":"Encontrada",
                reason:review?"Artista confirmado (ID Spotify); versão ao vivo (pedido: estúdio)":"Artista confirmado (ID Spotify); título idêntico",requestedArtist:true}
            generated.push({requestId:i+1,artist:i===0?"Veruca Salt":"Banda "+i,title:i===0?"Seether":"Música "+i,status:missing?"Não encontrada":review?"Versão alternativa":"Encontrada",
                match:missing?"NOT_FOUND":review?"ALTERNATE_VERSION":"EXACT_MATCH",section:missing?"wrongArtist":review?"alternate":"selected",reason:missing?"Nenhuma gravação com esse título; 1 resultado(s) de outros artistas separados":"",
                choice:missing?-1:0,selected:!missing&&!review,manual:false,ignored:false,batchId:"b1",candidates:missing?[homonym]:[own,homonym]})}rows=generated;selectedCount=25;dirty=generated.length>0;batches=[{batchId:"b1"}]}
        function search(){status="Revise as gravações antes de salvar."}
        function sectionOf(r){return r.ignored?"ignored":r.selected&&r.choice>=0?"selected":r.match==="ALTERNATE_VERSION"?"alternate":"wrongArtist"}
        function update(row,changes){let updated=rows.slice();updated[row]=Object.assign({},updated[row],changes);updated[row].section=sectionOf(updated[row]);rows=updated;selectedCount=updated.filter(r=>r.selected&&r.choice>=0).length}
        function choose(row,candidate,on){dirty=true;update(row,{choice:candidate,selected:on,manual:true,ignored:!on,status:on?"Escolha manual":"Ignorada"})}
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
        function init(){mock.dirty=false;mock.rows=[];mock.drafts=[];mock.batches=[];mock.selectedCount=0}
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
            const entries=wizard.versionEntries(mock.rows[2].candidates);compare(entries.length,4);verify(entries[0].header.indexOf("artista solicitado")>0);compare(entries[1].requestedArtist,true);verify(entries[2].header!==undefined);compare(entries[3].requestedArtist,false)
            const only=wizard.versionEntries(mock.rows[3].candidates);verify(only[0].header.indexOf("Nenhuma gravação do artista pedido")===0)
            mouseClick(next);tryCompare(wizard,"step",2)
            findChild(host,"importPlaylistName").text="Guitarras de hoje"
            findChild(host,"importPlaylistDescription").text="Uma coleção criada a partir de texto"
            grabImage(host).save("details.png")
            mouseClick(next);tryCompare(wizard,"step",3);compare(mock.details.name,"Guitarras de hoje");compare(mock.spotifyWrites,0)
            grabImage(host).save("local-saved.png")
            mouseClick(findChild(host,"playlistSendSpotify"));compare(mock.spotifyWrites,1)
            mock.dirty=false;wizard.close()
        }
        function test_draft_exit_and_more_songs(){
            wizard.fresh();const input=findChild(host,"playlistTextInput"),next=findChild(host,"playlistContinue");input.text="Scrawl - Charles"
            mouseClick(next);tryCompare(wizard,"step",1)
            // Exit with unsaved work: the dialog offers exactly three actions; "Continuar editando" keeps everything.
            wait(150);mouseClick(findChild(host,"playlistClose"));const exit=findChild(wizard,"exitDraftDialog");verify(exit!==null);tryCompare(exit,"opened",true)
            for(const name of ["exitSaveDraft","exitDiscard","exitKeepEditing"])verify(findChild(exit.contentItem,name)!==null,name)
            grabImage(host).save("exit-dialog.png")
            mouseClick(findChild(exit.contentItem,"exitKeepEditing"));tryCompare(exit,"opened",false);compare(wizard.visible,true);compare(mock.rows.length,30)
            // Esc does not discard either.
            keyClick(Qt.Key_Escape);tryCompare(exit,"opened",true);mouseClick(findChild(exit.contentItem,"exitKeepEditing"));tryCompare(exit,"opened",false);compare(mock.discards,0)
            // More songs for the same playlist.
            mouseClick(findChild(host,"addMoreSongs"));const add=findChild(wizard,"addSongsDialog");tryCompare(add,"opened",true)
            findChild(add.contentItem,"addSongsText").text="Nova A - Extra 1\nNova B - Extra 2\nNova C - Extra 3";mouseClick(findChild(add.contentItem,"addPreview"))
            tryVerify(()=>findChild(add.contentItem,"addPreviewText").text.indexOf("3 referência")===0)
            grabImage(host).save("add-songs.png")
            mouseClick(findChild(add.contentItem,"addConfirm"));tryCompare(add,"opened",false);compare(mock.rows.length,33);compare(mock.lastMode,"add");compare(wizard.filter,"batch")
            tryCompare(findChild(host,"reviewList"),"count",3);verify(findChild(host,"undoBatch").visible)
            grabImage(host).save("review-added.png")
            // Save draft and leave; the entry screen then offers "Continuar rascunho".
            mouseClick(findChild(host,"playlistClose"));tryCompare(exit,"opened",true);mouseClick(findChild(exit.contentItem,"exitSaveDraft"))
            tryCompare(wizard,"visible",false);compare(mock.savedDrafts,1)
            wizard.fresh();tryCompare(wizard,"visible",true);compare(wizard.step,0);const panel=findChild(host,"draftsPanel");verify(panel.visible)
            grabImage(host).save("drafts-entry.png")
            mouseClick(findChild(panel,"draftContinue"));tryCompare(wizard,"step",1)
            // Leave without saving: the saved draft stays.
            mock.dirty=true;mouseClick(findChild(host,"playlistClose"));tryCompare(exit,"opened",true);mouseClick(findChild(exit.contentItem,"exitDiscard"))
            tryCompare(wizard,"visible",false);compare(mock.discards,1);compare(mock.drafts.length,1)
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

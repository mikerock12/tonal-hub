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
        function cancel(){}
        function parse(text,format,artist){let generated=[];for(let i=0;i<30;i++){let missing=i%7===3;generated.push({artist:i===0?"Veruca Salt":"Banda "+i,title:i===0?"Seether":"Música "+i,status:missing?"Não encontrada":"Encontrada",choice:missing?-1:0,selected:!missing,candidates:missing?[]:[{title:"Música "+i,artist:"Banda "+i,album:"Álbum sintético"}]})}rows=generated;selectedCount=26}
        function search(){status="Revise as gravações antes de salvar."}
        function choose(row,candidate,on){let updated=rows.slice();updated[row]=Object.assign({},updated[row],{choice:candidate,selected:on,status:on?"Revisada":"Ignorada"});rows=updated;selectedCount=updated.filter(r=>r.selected&&r.choice>=0).length}
        function edit(row,artist,title){}
        function move(row,direction){}
        function clearCover(){cover=""}
        function setCover(url){return true}
        function saveLocal(name,description,dedup){savedId="local-fixture";details={name:name,description:description,cover:"",tracks:["uri"]};status="Playlist salva no Tonalify. Deseja salvar também no Spotify?";return true}
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
            const filter=findChild(host,"missingFilter"),list=findChild(host,"reviewList");verify(filter!==null&&list!==null);tryCompare(list,"count",30);compare(filter.text,"Só não encontradas (4)")
            mouseClick(filter);tryCompare(list,"count",4);mouseClick(filter);tryCompare(list,"count",30)
            list.contentY=480;wait(60);const beforeY=list.contentY;wizard.chooseReview(1,0,false);wait(80);compare(list.contentY,beforeY);compare(mock.selectedCount,25)
            grabImage(host).save("review.png")
            mouseClick(next);tryCompare(wizard,"step",2)
            findChild(host,"importPlaylistName").text="Guitarras de hoje"
            findChild(host,"importPlaylistDescription").text="Uma coleção criada a partir de texto"
            grabImage(host).save("details.png")
            mouseClick(next);tryCompare(wizard,"step",3);compare(mock.details.name,"Guitarras de hoje");compare(mock.spotifyWrites,0)
            grabImage(host).save("local-saved.png")
            mouseClick(findChild(host,"playlistSendSpotify"));compare(mock.spotifyWrites,1)
            wizard.close()
        }
    }
}

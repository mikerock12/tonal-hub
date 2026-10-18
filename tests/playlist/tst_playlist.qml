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
        function parse(text,format,artist){rows=[{artist:"Veruca Salt",title:"Seether",status:"Encontrada",choice:0,selected:true,candidates:[{title:"Seether",artist:"Veruca Salt",album:"American Thighs"}]},{artist:"L7",title:"Shove",status:"Revisar versão",choice:0,selected:false,candidates:[{title:"Shove",artist:"L7",album:"Smell the Magic"}]}];selectedCount=1}
        function search(){status="Revise as gravações antes de salvar."}
        function choose(row,candidate,on){}
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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: panel
    required property var controller
    parent:Overlay.overlay
    modal:true
    title:"Jam com amigos"
    width:Math.min(580,parent?parent.width-40:540)
    height:Math.min(720,parent?parent.height-60:680)
    anchors.centerIn:parent
    standardButtons:Dialog.Close
    function present(){open();controller.refresh()}
    contentItem:ScrollView {clip:true;ColumnLayout {
        width:panel.availableWidth;spacing:14
        Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:"Iniciar Jam cria a sessão; seus amigos entram pelo convite (link). Ao iniciar, o convite é copiado para você colar na conversa que escolher. As entradas são informadas; não há aprovação prévia de participantes nesta integração experimental."}
        Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:panel.controller.status;font.bold:true}
        BusyIndicator {running:panel.controller.busy;visible:running;Layout.alignment:Qt.AlignHCenter}
        Flow {
            Layout.fillWidth:true;spacing:8
            Button {text:"Iniciar Jam";enabled:!panel.controller.busy&&!panel.controller.session.active;onClicked:panel.controller.start()}
            Button {text:"Atualizar";enabled:!panel.controller.busy;onClicked:panel.controller.refresh()}
            Button {text:"Copiar convite";visible:!!panel.controller.session.link;onClicked:panel.controller.copyInvite()}
            Button {text:panel.controller.session.host?"Encerrar Jam":"Sair do Jam";visible:!!panel.controller.session.active;enabled:!panel.controller.busy;onClicked:panel.controller.leave()}
        }
        TextField {objectName:"jamInviteLink";Layout.fillWidth:true;readOnly:true;selectByMouse:true;visible:!!panel.controller.session.link;text:panel.controller.session.link||""}
        Label {text:"Participantes";font.bold:true;visible:!!panel.controller.session.active}
        Repeater {model:panel.controller.session.members||[];Label {required property var modelData;Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:modelData.name}}
        TextField {id:invite;Layout.fillWidth:true;maximumLength:600;placeholderText:"Cole um convite open.spotify.com/socialsession…"}
        Button {text:"Entrar pelo convite";enabled:!panel.controller.busy&&invite.text.trim().length>0;onClicked:panel.controller.join(invite.text)}
        Label {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:11;text:"O Spotify pode exigir Premium e alterar esse protocolo. Avisos de fila mostram a autoria apenas quando identificada pelo serviço. Nenhum convite é enviado automaticamente."}
    }}
}

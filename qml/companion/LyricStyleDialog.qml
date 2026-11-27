import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "LyricColors.js" as LyricColors

Dialog {
    id: dialog
    objectName: "lyricStyleDialog"
    required property var controller
    parent: Overlay.overlay;anchors.centerIn:parent
    title:"Fontes e cores das letras";modal:true
    width:Math.min(640,parent?parent.width-32:600)
    height:Math.min(760,parent?parent.height-32:700)
    standardButtons:Dialog.Close
    // Rainbow rail with − / + at the ends: 1001 positions over 16 color stops (0 = branco … 1000 = preto).
    component ColorRail: ColumnLayout {
        id: rail
        required property string targetKey
        required property string label
        property int position: LyricColors.nearest(dialog.controller.lyricStyle[targetKey])
        // Until the rail is touched, show the stored color itself (older colors sit between rail positions).
        property bool touched: false
        readonly property string color: touched?LyricColors.railColor(position):String(dialog.controller.lyricStyle[targetKey] || LyricColors.railColor(position)).toUpperCase()
        function commit(){touched=true;dialog.controller.setLyricStyle(targetKey,LyricColors.railColor(position))}
        function nudge(delta){touched=true;position=Math.max(0,Math.min(LyricColors.railMax,position+delta));commit()}
        Layout.fillWidth:true;spacing:4
        RowLayout{Layout.fillWidth:true
            Label{text:rail.label;color:"#a8b0b7";Layout.fillWidth:true}
            Rectangle{width:28;height:20;radius:6;color:rail.color;border.color:"#88ffffff"}
            Label{objectName:rail.targetKey+"Value";text:rail.color+" · "+rail.position;font.family:"Consolas"}
        }
        RowLayout{Layout.fillWidth:true;spacing:8
            Button{objectName:rail.targetKey+"Minus";text:"−";implicitWidth:40;autoRepeat:true;autoRepeatDelay:400;autoRepeatInterval:60;Accessible.name:rail.label+": menos 1";onClicked:rail.nudge(-1)}
            Slider{
                id:slider;objectName:rail.targetKey+"Rail";Layout.fillWidth:true;from:0;to:LyricColors.railMax;stepSize:1;snapMode:Slider.SnapAlways
                value:rail.position;Accessible.name:rail.label+": posição de 0 a 1000 no arco-íris"
                onMoved:{rail.touched=true;rail.position=Math.round(value)}
                onPressedChanged:if(!pressed)rail.commit()
                background:Rectangle{
                    x:slider.leftPadding;y:slider.topPadding+slider.availableHeight/2-height/2;width:slider.availableWidth;height:16;radius:8;border.color:"#66ffffff"
                    gradient:Gradient{orientation:Gradient.Horizontal
                        GradientStop{position:0/15;color:"#FFFFFF"}
                        GradientStop{position:1/15;color:"#FF0000"}
                        GradientStop{position:2/15;color:"#FF4000"}
                        GradientStop{position:3/15;color:"#FF8000"}
                        GradientStop{position:4/15;color:"#FFBF00"}
                        GradientStop{position:5/15;color:"#FFFF00"}
                        GradientStop{position:6/15;color:"#80FF00"}
                        GradientStop{position:7/15;color:"#00FF00"}
                        GradientStop{position:8/15;color:"#00FF80"}
                        GradientStop{position:9/15;color:"#00FFFF"}
                        GradientStop{position:10/15;color:"#0080FF"}
                        GradientStop{position:11/15;color:"#0000FF"}
                        GradientStop{position:12/15;color:"#8000FF"}
                        GradientStop{position:13/15;color:"#FF00FF"}
                        GradientStop{position:14/15;color:"#FF0080"}
                        GradientStop{position:15/15;color:"#000000"}
                    }
                }
                handle:Rectangle{x:slider.leftPadding+slider.visualPosition*(slider.availableWidth-width);y:slider.topPadding+slider.availableHeight/2-height/2;width:22;height:28;radius:6;color:rail.color;border.color:"white";border.width:2}
            }
            Button{objectName:rail.targetKey+"Plus";text:"+";implicitWidth:40;autoRepeat:true;autoRepeatDelay:400;autoRepeatInterval:60;Accessible.name:rail.label+": mais 1";onClicked:rail.nudge(1)}
        }
    }
    ScrollView{anchors.fill:parent;clip:true
        ColumnLayout{width:dialog.availableWidth-18;spacing:20
            Repeater{model:[{prefix:"lyric",label:"Letra original"},{prefix:"translation",label:"Tradução"}]
                ColumnLayout{
                    id:section
                    required property var modelData
                    readonly property string prefix:modelData.prefix
                    Layout.fillWidth:true;spacing:10
                    Label{text:section.modelData.label;font.pixelSize:22;font.bold:true}
                    RowLayout{Layout.fillWidth:true
                        // Each font is listed in its own typeface.
                        ComboBox{
                            id:fontBox;objectName:section.prefix+"FontBox";Layout.fillWidth:true;implicitHeight:48;model:LyricColors.fonts
                            currentIndex:Math.max(0,LyricColors.fonts.indexOf(dialog.controller.lyricStyle[section.prefix+"Font"]))
                            onActivated:index=>dialog.controller.setLyricStyle(section.prefix+"Font",LyricColors.fonts[index])
                            contentItem:Label{leftPadding:12;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight;text:LyricColors.fontLabel(fontBox.currentValue)+" · Aa Bb Çç";font.family:LyricColors.family(fontBox.currentValue);font.weight:LyricColors.weight(fontBox.currentValue,false);font.capitalization:fontBox.currentValue==="Versalete"?Font.SmallCaps:Font.MixedCase;font.pixelSize:18}
                            delegate:ItemDelegate{required property string modelData;required property int index;width:ListView.view?ListView.view.width:fontBox.width;height:52;highlighted:fontBox.highlightedIndex===index
                                contentItem:Label{text:LyricColors.fontLabel(modelData)+" · Aa Bb Çç 123";font.family:LyricColors.family(modelData);font.weight:LyricColors.weight(modelData,false);font.capitalization:modelData==="Versalete"?Font.SmallCaps:Font.MixedCase;font.pixelSize:20;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
                                Accessible.name:"Fonte "+LyricColors.fontLabel(modelData)}
                        }
                        SpinBox{from:16;to:44;value:dialog.controller.lyricStyle[section.prefix+"Size"];onValueModified:dialog.controller.setLyricStyle(section.prefix+"Size",value);Accessible.name:"Tamanho"}
                    }
                    // Each effect is previewed with the current colors and font.
                    ComboBox{
                        id:effectBox;objectName:section.prefix+"EffectBox";Layout.fillWidth:true;implicitHeight:56;model:LyricColors.effects
                        currentIndex:Math.max(0,LyricColors.effects.indexOf(dialog.controller.lyricStyle[section.prefix+"Effect"]))
                        onActivated:index=>dialog.controller.setLyricStyle(section.prefix+"Effect",LyricColors.effects[index])
                        contentItem:Item{clip:true
                            // Effects are placed through a wrapper: MultiEffect output follows the parent origin, not the item's own x.
                            Item{x:14;width:parent.width-28;height:parent.height
                            StyledVerse{anchors.verticalCenter:parent.verticalCenter;width:parent.width;wrapMode:Text.NoWrap;elide:Text.ElideRight;text:effectBox.currentValue;appearance:dialog.controller.lyricStyle;translation:section.prefix==="translation";effectOverride:effectBox.currentValue;font.pixelSize:22}}}
                        delegate:ItemDelegate{required property string modelData;required property int index;width:ListView.view?ListView.view.width:effectBox.width;height:58;highlighted:effectBox.highlightedIndex===index
                            background:Rectangle{color:highlighted?"#2a3a46":"#1d2328"}
                            contentItem:Item{clip:true
                                Item{x:8;width:parent.width-16;height:parent.height
                                StyledVerse{objectName:"effectPreview";anchors.verticalCenter:parent.verticalCenter;width:parent.width;wrapMode:Text.NoWrap;text:modelData;appearance:dialog.controller.lyricStyle;translation:section.prefix==="translation";effectOverride:modelData;font.pixelSize:22}}}
                            Accessible.name:"Efeito "+modelData}
                    }
                    RowLayout{Repeater{model:[{key:"Bold",label:"Negrito"},{key:"Italic",label:"Itálico"},{key:"Underline",label:"Sublinhar"}];CheckBox{required property var modelData;text:modelData.label;checked:!!dialog.controller.lyricStyle[section.prefix+modelData.key];onToggled:dialog.controller.setLyricStyle(section.prefix+modelData.key,checked)}}}
                    ColorRail{targetKey:section.prefix+"Color";label:"Cor do texto"}
                    ColorRail{targetKey:section.prefix+"OutlineColor";label:"Contorno / brilho / sombra"}
                }
            }
        }
    }
}

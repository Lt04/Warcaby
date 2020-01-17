import socket
from struct import pack, unpack
from PyQt5 import QtGui
from PyQt5 import QtCore
from PyQt5.QtWidgets import *
from PyQt5 import QtWidgets
from PyQt5.QtGui import *
import sys
import math

HOST = socket.gethostbyname(input("Podaj nazwę hosta"))
PORT = int(input("Podaj numer portu"))

szachownica = []
turinfo = ""
szachownica_format = "64c"

#klasa klienta z atrybutami odpowiadającymi tym ze struktury przesyłanej przez serwer
class GameClient:
    cmd = 0
    room_id = 0
    client_id = 0
    move_from = b"--"
    move_to = b"--"

    msgFormat = "BBH2s2s"
    clientSocket = None

    #nawiązywanie połączenia
    def __init__(self, serverAddress, serverPort):
        self.clientSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.clientSocket.connect((serverAddress, serverPort))
        response = self.sendMsg(1)
        self.client_id = response["client_id"]
        self.room_id = response["room_id"]

    #pakowanie wiadomości dla serwera do wygodnego dla niego formatu
    def getMsgAsBytes(self):
        return pack(self.msgFormat, self.cmd, self.room_id, self.client_id, self.move_from, self.move_to)

    #rozpakowywanie wiadomości od serwera
    def msgFromBytes(self, messageBytes):
        messageTuple = unpack(self.msgFormat, messageBytes)
        response = {
            "cmd": messageTuple[0],
            "room_id": messageTuple[1],
            "client_id": messageTuple[2],
            "move_from": messageTuple[3],
            "move_to": messageTuple[4]
        }
        return response

    #wysyłanie wiadomości
    def sendMsg(self, cmd, move_from=b"--", move_to=b"--"):
        global szachownica
        global turinfo
        self.cmd = cmd
        self.move_from = move_from
        self.move_to = move_to
        self.clientSocket.sendall(self.getMsgAsBytes())
        if cmd == 4:
            szachownica = self.clientSocket.recv(64)
            turinfo = self.clientSocket.recv(19)
        response = self.msgFromBytes(self.clientSocket.recv(128))
        if cmd != 4:
            print("Odpowiedź serwera:\r\ncmd\t\t:", response["cmd"], "\r\nroom_id\t\t:", response["room_id"],
                "\r\nclient_id\t:", response["client_id"], "\r\nmove_from\t:", response["move_from"], "\r\nmove_to\t\t:",
                response["move_to"])
        return response


#klasa GUI
class App(QWidget):

    def __init__(self):
        super().__init__()
        self.title = 'Warcaby'
        self.setFixedSize(340,470)
        self.label = QLabel(self)
        self.initUI()

    def initUI(self):
        self.setWindowTitle(self.title)
        b1 = self.initButton("Wyślij ruch", 20, 400, self.onClick1)
        b2 = self.initButton("Zrezygnuj", 110, 400, self.onClick2)
        b3 = self.initButton("Wyjdź", 200, 400, self.onClick3)
        self.buttons = [b1, b2]
        b1.resize(90, 50)
        b2.resize(90, 50)
        b3.resize(90, 50)
        b3.setStyleSheet("background-color: red")
        self.textbox = QLineEdit(self)
        self.textbox.move(40, 320)
        self.textbox.resize(50, 50)
        self.textbox2 = QLineEdit(self)
        self.textbox2.move(140, 320)
        self.textbox2.resize(50, 50)
        font = self.textbox.font()
        font.setPointSize(20)
        self.textbox.setFont(font)
        self.textbox.setAlignment(QtCore.Qt.AlignCenter)
        self.textbox2.setFont(font)
        self.textbox2.setAlignment(QtCore.Qt.AlignCenter)
        self.output_rd = QtWidgets.QTextBrowser(self)
        self.output_rd.setGeometry(QtCore.QRect(10, 50, 300, 270))
        self.output_rd2 = QtWidgets.QTextBrowser(self)
        self.output_rd2.setGeometry(QtCore.QRect(10, 0, 300, 30))
        self.pole1 = QtWidgets.QTextBrowser(self)
        self.pole1.setGeometry(QtCore.QRect(20, 370, 90, 25))
        self.pole1.append("Pole początkowe")
        self.pole2 = QtWidgets.QTextBrowser(self)
        self.pole2.setGeometry(QtCore.QRect(130, 370, 75, 25))
        self.pole2.append("Pole końcowe")
        self.update()
        self.show()

    def initButton(self, name, left, top, function):
        button = QPushButton(name, self)
        button.setGeometry(left, top, 200, 150)
        button.clicked.connect(function)
        button.setStyleSheet('background-color: white; color: black')
        font = QtGui.QFont("Calibri", 12)
        font.setBold(True)
        button.setFont(font)

        return button

    def onClick1(self):
        textboxValue = self.textbox.text()
        textbox2Value = self.textbox2.text()
        client.sendMsg(0, textboxValue.encode(), textbox2Value.encode())
        self.textbox.clear()
        self.textbox2.clear()
        return client

    def onClick2(self):
        client.sendMsg(3)
        return client

    #Uaktualnianie informacji na temat stanu szachownicy
    def update(self):
        global szachownica
        global turinfo
        szach_ost = []
        client.sendMsg(4)
        linia = ""
        string = '\n' + "          " + "A" + "   " + "||" + "   " + "B" + "   " + "||" + "   " + "C" + "   " + "||" + "   " + "D" + "   " + "||" + "   " + "E" + "   " + "||" + "   " + "F" + "   " + "||" + "   " + "G" + "   " + "||" + "   " + "H" + "   " + "||" + "   " + '\n'
        licznik = 1
        szach_tuple = unpack(szachownica_format, szachownica)
        for i in szach_tuple:
            szach_ost.append(i)
        for i in range(64):
            if szach_ost[i] == b'\x00':
                x = "0"
            elif szach_ost[i] == b'\x01':
                x = "1"
            elif szach_ost[i] == b'\x02':
                x = "2"
            elif szach_ost[i] == b'\x03':
                x = "3"
            elif szach_ost[i] == b'\x04':
                x = "4"
            linia = linia +  x  + "   " + "||" + "   "
            if licznik % 8 == 0:
                linia =  str(math.floor(i/8) + 1) + "   " + "||" + "   " + linia + '\n' + "---------------------------------------------------------------------" + '\n'
                string = linia + string
                linia = ""
            licznik = licznik + 1
        self.output_rd.clear()
        self.output_rd.append(string)
        self.output_rd2.clear()
        self.output_rd2.append(turinfo.decode())
        return client
    def onClick3(self):
        client.sendMsg(2)
        exit(0)

if __name__ == '__main__':
    client = GameClient(HOST, PORT)
    app = QApplication(sys.argv)
    ex = App()
    timer = QtCore.QTimer()
    timer.timeout.connect(ex.update)
    timer.start(500)
    sys.exit(app.exec_(), client.sendMsg(2), exit(0))
    
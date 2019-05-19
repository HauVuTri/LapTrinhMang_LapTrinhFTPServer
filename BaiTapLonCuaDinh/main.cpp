#include <iostream>
#include <WinSock2.h>
#include <vector>
#include <fstream>
#include <string>
#include<time.h>
#include <algorithm>

#define NAME_LENGTH 50
#define PATH_LENGTH 1024
#define IP_SERVER "127.0.0.1"

using namespace std;

int cnum = 0;

struct User {
	char username[NAME_LENGTH] = "";
	char password[NAME_LENGTH] = "";
	char h_path[PATH_LENGTH] = "";
};

vector<User *> users;	// danh sach users

void readConfigFile() {	// doc file config va luu vao list users
	
	ifstream fileInput("config.txt");
	if (fileInput.fail()) {
		cout << "Loi khi doc file config.txt" << endl;
		system("pause");
		exit(-1);
	}
	while (!fileInput.eof()) { // đọc từng dòng và tách lấy thông tin lưu trong các User
		User * user = new User;
		char tmp[255] = "";
		fileInput.getline(tmp, 255);
		sscanf(tmp, "%s%s", user->username, user->password);
		char * secondSpace = strchr(strchr(tmp, ' ') + 1, ' ');
		strcpy(user->h_path, secondSpace + 1);
		users.push_back(user);
	}
	fileInput.close();
}

void clear() { // giải phóng bộ nhớ của các biến lưu trữ User
	for (int i = 0; i < users.size(); i++) {
		delete(users.at(i));

	}
}

void strtrim(char * str) { // loại bỏ khoảng trống ở đầu và cuối chuỗi
	while (str[strlen(str) - 1] == ' ') str[strlen(str) - 1] = '\0';
	int i = 0;
	while (str[i] == ' ') i++;
	strcpy(str, str + i);
}

bool splitRequestCommand(const char * requestCmd, char * cmd, char * args) { // tách yêu cầu thành Command và tham số đầu vào.
	char tmp[1024] = "";
	strcpy(tmp, requestCmd);
	while (tmp[strlen(tmp) - 1] == '\n' || tmp[strlen(tmp) - 1] == '\r') tmp[strlen(tmp) - 1] = '\0'; // bỏ qua \r\n.
	char * firstSpace = strchr(tmp, ' '); // tìm sp đầu tiên
	if (firstSpace == NULL) {
		if (strlen(tmp) > 4) return false; // mã command tối đa 4 kí tự
		strcpy(cmd, tmp);
		strupr(cmd);
		strcpy(args, "");
		return true;
	}
	else if (firstSpace - requestCmd > 4) { // mã command tối đa 4 kí tự
		return false;
	}
	else {
		strcpy(args, firstSpace + 1);
		*(firstSpace) = '\0';
		strtrim(args);
		if (strlen(tmp) > 4) return false;
		strcpy(cmd, tmp);
		strupr(cmd);
		return true;
	}
}

void processPathname(const char * cwd, const char * home, char * args, char * v_path, char * p_path) {
	// xu ly args de lay ra duong dan.
	// thực hiện ghép đường dẫn trong tham số args với cwd + home để ra được đường dẫn lưu file trong máy
	// trả về v_path: đường dẫn login từ root của file(home directory của user coi là root), p_path: đường dẫn lưu trữ file trên máy
	if (strlen(args) == 0 || strcmp(args, "-l") == 0) strcpy(v_path, cwd); // path bỏ trống, lấy path trong cwd
	else if (args[0] == '/') strcpy(v_path, args);// đường dẫn tuyệt đối
	else sprintf(v_path, "%s%s", cwd, args); // đường dẫn tương đối dạng folder1/
	while (v_path[strlen(v_path) - 1] == ' ' || v_path[strlen(v_path) - 1] == '/') v_path[strlen(v_path) - 1] = '\0';
	sprintf(p_path, "%s%s", home, v_path);
}

int findFile(const char * fullpath) { // trả về 0 nếu không tìm thấy, trả về 1 nếu là thư mục, trả về 2 nếu là file
	WIN32_FIND_DATAA FDATA;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	hFind = FindFirstFileA(fullpath, &FDATA);
	if (hFind == INVALID_HANDLE_VALUE) return 0; // không tìm thấy
	if (FindNextFileA(hFind, &FDATA) == 0) {
		if (FDATA.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 1; // tìm được 1 thư mục
		else return 2; // tìm được 1 file
	}
	else return 2; // tìm được nhiều hơn 1 file/thư mục
}

int sizeOfFile(const char * fullpath) { // trả về kích thươc file nếu tìm thấy, trả về 0 nếu không tìm thấy
	WIN32_FIND_DATAA FDATA;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	hFind = FindFirstFileA(fullpath, &FDATA);
	if (hFind == INVALID_HANDLE_VALUE) return 0;
	int size = FDATA.nFileSizeHigh * (MAXDWORD + 1) + FDATA.nFileSizeLow;
	if (FindNextFileA(hFind, &FDATA) == 0) {
		if (FDATA.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 0; // tìm được 1 thư mục
		else return size; // tìm được 1 file
	}
	else return 0; // tìm được nhiều hơn 1 file/thư mục
}

bool construcListCmdData(const char * fullpath, User * user, char ** data) { // tạo data cho lệnh list
	int type = findFile(fullpath);
	if (type == 0) return false; // không tìm thấy file / thư mục
	else if (type == 1) { // là thư mục
		char tmp[2 * PATH_LENGTH] = "";
		sprintf(tmp, "%s/*.*", fullpath); // thêm chuỗi này vào cuối đường dẫn sẽ tìm tất cả file/thư mục con trong thư mục này
		WIN32_FIND_DATAA FDATA;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		hFind = FindFirstFileA(tmp, &FDATA);
		// tạo dữ liệu theo định dạng giống linux như quyền truy cập, ngày tháng, chủ + group,... (giả định)
		char metadata[100] = "";
		sprintf(metadata, "rwxr-xr-x %d %s %s ", 1, user->username, user->username);
		char month[12][4] = { "Jan","Feb","Mar","Apr","May","Jun","Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
		int data_size = 1; // kich thuoc cua du lieu
		do {
			SYSTEMTIME stLocal;
			FileTimeToSystemTime(&FDATA.ftLastAccessTime, &stLocal);
			if (FDATA.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { // là thư mục
				// trong kết quả sẽ xuất hiện 2 thư mục đó là thư mục . (thư mục hiện tại) và thư mục .. (cha) cần phải loại bỏ 2 thư mục này khỏi data
				if (strcmp(FDATA.cFileName, ".") && strcmp(FDATA.cFileName, "..")) { // nếu tên thư mục khác với . và .. thì thực hiện thêm vào data
					char line[1024] = "";
					// đoạn dưới này là định dạng dữ liệu theo tương tự như trong linux ( kết quả lệnh ls)
					sprintf(line, "d%s%10d %s %02d %04d %s\r\n", metadata, FDATA.nFileSizeHigh * (MAXDWORD + 1) + FDATA.nFileSizeLow, month[stLocal.wMonth], stLocal.wDay, stLocal.wYear, FDATA.cFileName);
					data_size += strlen(line);
					*data = (char *)realloc(*data, data_size);
					strcat(*data, line);
				}
			}
			else { // là file
				char line[1024] = "";
				sprintf(line, "-%s%10d %s %02d %04d %s\r\n", metadata, FDATA.nFileSizeHigh * (MAXDWORD + 1) + FDATA.nFileSizeLow, month[stLocal.wMonth], stLocal.wDay, stLocal.wYear, FDATA.cFileName);
				data_size += strlen(line);
				*data = (char *)realloc(*data, data_size);
				strcat(*data, line);
			}
		} while (FindNextFileA(hFind, &FDATA) != 0);
		return true;
	}
}

DWORD WINAPI clientThread(LPVOID p) {
	SOCKET c = (SOCKET)p;
	User * user;	// trỏ đến cấu trúc user ứng với client này
	char username[NAME_LENGTH] = "";
	char password[NAME_LENGTH] = "";
	// gửi message
	char message[] = "220 Simple FTP Server.\r\n";
	send(c, message, strlen(message), 0);
	int trans_mode = 0; // 0: chưa set, 2: passive mode
	// socket kết nối kênh dữ liệu sử dụng để truyền dữ liệu file hoặc dữ liệu lệnh list ở trên (theo giao thức FTP)
	SOCKET sdata = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SOCKADDR_IN addr_data;
	addr_data.sin_family = AF_INET;
	bool login = false; //biến lưu trạng thái đăng nhập
	char cwd[PATH_LENGTH] = "/"; // thư mục hiện tại
	while (true) {
		char rbuf[1024];
		memset(rbuf, 0, 1024);
		if (recv(c, rbuf, 1023, 0) <= 0) break; // nhận lệnh từ client,trả về số byte nhận được, <= 0 thì tức là client đã ngắt kết nối
		char cmd[5] = "";
		char args[1024] = "";
		if (!splitRequestCommand(rbuf, cmd, args)) { // tách lấy cmd và args
			char sbuf[] = "500 Systax error, command unrecognized.\r\n";
			send(c, sbuf, strlen(sbuf), 0);
			continue;
		}

		//USER
		if (strcmp(cmd, "USER") == 0) {	// lenh USER<SP><username><\n>
			if (strlen(args) > NAME_LENGTH) { // username quá dài
				char sbuf[] = "501 Username too long.\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else {
				if (login) login = false;	// phiên đăng nhập mới
				if (strlen(args) == 0) {
					// nhận được username trống 
					// gửi mã 501: lỗi cú pháp ở tham số lệnh
					char sbuf[] = "501 Syntax error!\r\n";
					send(c, sbuf, strlen(sbuf), 0);
				}
				else {
					strcpy(username, args); // lấy username: USER<SP><username>\n
					char sbuf[1024] = "";
					sprintf(sbuf, "331 Password required for %s\r\n", username);
					send(c, sbuf, strlen(sbuf), 0);
				}
			}
		}

		//PASS
		else if (strcmp(cmd, "PASS") == 0) {	// lenh PASS<sp><password><\n>
			if (strlen(args) > NAME_LENGTH) { // password quá dài
				char sbuf[] = "501 Password too long.\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else if (!login) { // neu chua login thi kiem tra tai khoan mat khau 
				strcpy(password, args); // lay password;
				// tim trong danh sach user xem co trung username va password khong?
				for (int i = 0; i < users.size(); i++) {
					// password = null tuong ung voi moi pass
					if (strcmp(username, users.at(i)->username) == 0 && (strcmp(password, users.at(i)->password) == 0 || strcmp("null", users.at(i)->password) == 0)) {
						// neu tim thay thi dang nhap thanh cong
						char sbuf[] = "230 Logged on.\r\n";
						send(c, sbuf, strlen(sbuf), 0);
						login = true;
						user = users.at(i);
						break;
					}
				}
				if (!login) { // sau qua trinh kiem tra van khong login duoc
					// gui ma loi tai khoan hoac mat khau khong chinh xac
					char sbuf[] = "530 User or Password incorrect!.\r\n";
					send(c, sbuf, strlen(sbuf), 0);
				}
			}
			else { // dang login ma gui PASS
				// gui ma 503: dang login ma gui command PASS, thi bao loi cu phap
				char sbuf[] = "503 Bad sequence of command.\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
		}

		//PASSIVE
		else if (strcmp(cmd, "PASV") == 0) {
			if (!login) {
				// yêu cầu login 
				char sbuf[] = "530 Please login with USER and PASS!.\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else {
				closesocket(sdata);
				sdata = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				// lưu cấu trúc địa chỉ
				addr_data.sin_addr.s_addr = 0;
				srand(time(NULL));
				// đoạn dưới sẽ thực hiện lắng nghe trên socket mới để lúc sau truyền dữ liệu
				addr_data.sin_port = htons(rand() % (65535 - 1024 + 1) + 1024);	// sinh port ngẫu nhiên trong khoảng từ 1024 -> 65535
				bind(sdata, (sockaddr*)&addr_data, sizeof(addr_data));
				listen(sdata, 5);
				trans_mode = 2;
				// tạo câu lệnh passive và gửi đi
				char sbuf[1024] = "";
				char ip[] = IP_SERVER;
				replace(ip, ip + strlen(ip) + 1, '.', ','); // chuyển dấu . thành ,
				sprintf(sbuf, "227 Entering Passive Mode (%s,%d,%d)\r\n", ip, ntohs(addr_data.sin_port) / 256, ntohs(addr_data.sin_port) % 256);
				send(c, sbuf, strlen(sbuf), 0); // gửi câu lệnh
			}
		}
		else if (strcmp(cmd, "PWD") == 0) { // xử lý lệnh PWD<sp><\r\n>
			if (!login) {
				char sbuf[] = "530 Please login with USER and PASS!\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else { // trả về cwd, thư mục hiện tại
				char sbuf[1024] = "";
				sprintf(sbuf, "257 \"%s\" is current directory.\r\n", cwd);
				send(c, sbuf, strlen(sbuf), 0);
			}
		}
		else if (strcmp(cmd, "CWD") == 0) { // xử lý lệnh CWD<sp><pathname><\r\n>
			if (!login) { // chưa login thì không được sử dụng
				char sbuf[] = "530 Please login with USER and PASS!\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else if (strlen(args) == 0) { // tham số trống -> đường dẫn như cũ
				char sbuf[1024] = "";
				sprintf(sbuf, "250 missing argument to CWD. \"%s\" is current directory.\r\n", cwd);
				send(c, sbuf, strlen(sbuf), 0);
			}
			else {
				char v_path[PATH_LENGTH] = "";
				char p_path[PATH_LENGTH] = "";
				processPathname(cwd, user->h_path, args, v_path, p_path);
				if (findFile(p_path) == 1) { // tìm xem có chứa thư mục này hay không
					// là thư mục, gửi lệnh ok
					sprintf(cwd, "%s/", v_path);
					char sbuf[1024] = "";
					sprintf(sbuf, "250 successful. \"%s\" is current directory.\r\n", cwd);
					send(c, sbuf, strlen(sbuf), 0);
				}
				else {
					char sbuf[1024] = "";
					sprintf(sbuf, "550 CWD failed. \"%s\": directory not found.\r\n", args);
					send(c, sbuf, strlen(sbuf), 0);
				}

			}
		}


		//LỆNH LIST
		else if (strcmp(cmd, "LIST") == 0) { // xử lý lệnh LIST<sp><path><\n>
			if (!login) { // yêu cầu login trước
				char sbuf[] = "530 Please login with USER and PASS!\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else if (trans_mode == 0) { // phải có lệnh đặt chế độ trước khi truyền dữ liệu
				char sbuf[] = "425 Use PASV or PORT command first.\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else {
				char v_path[PATH_LENGTH] = "";
				char p_path[PATH_LENGTH] = "";
				processPathname(cwd, user->h_path, args, v_path, p_path);
				char * data = (char *)malloc(1);
				*data = '\0';
				if (construcListCmdData(p_path, user, &data)) {
					// mở kết nối mới và gửi dữ liệu đi
					// gửi mã 150
					char sbuf[1024] = "";
					sprintf(sbuf, "150 Opening data channel for directory listing of %s\r\n", args);
					send(c, sbuf, strlen(sbuf), 0);
					// chấp nhận kết nối đến socket sdata vừa tạo ở trên lệnh PASV, được socket sd1 và gửi dữ liệu đi rồi đóng kết nối trên socket này
					SOCKADDR_IN caddr_t;
					int clen_t = sizeof(caddr_t);
					SOCKET sd1 = accept(sdata, (sockaddr*)&caddr_t, &clen_t);
					send(sd1, data, strlen(data), 0);
					closesocket(sd1); // đóng kết nối
					// gửi lệnh báo thành công
					strcpy(sbuf, "226 Successful transfering\r\n");
					send(c, sbuf, strlen(sbuf), 0);
					trans_mode = 0;
				}
				else {
					// không tìm thấy, gửi thông báo lỗi
					// gửi mã 550 File or Directory not found.
					char sbuf[] = "550 File or Directory not found.\r\n";
					send(c, sbuf, strlen(sbuf), 0);
				}
				free(data);
			}
		}



		//LỆNH RETRIEVE: TẢI DỮ LIỆU Từ SERVER về
		else if (strcmp(cmd, "RETR") == 0) { // tải dữ liệu từ server
			if (!login) { // yêu cầu login 
				char sbuf[] = "530 Please login with USER and PASS!\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else if (strlen(args) == 0) { // không được bỏ trống tên file
				char sbuf[] = "501 Syntax error\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else if (trans_mode == 0) {
				char sbuf[] = "425 Use PASV or PORT command first.\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else {
				char v_path[PATH_LENGTH] = "";
				char p_path[PATH_LENGTH] = "";
				processPathname(cwd, user->h_path, args, v_path, p_path);
				if (findFile(p_path) == 2) { //tìm thấy file thì đọc và gửi đi dạng nhị phân
					bool canOpenFile = true;
					FILE * f = fopen(p_path, "rb"); // mở file dạng nhị phân
					if (f != NULL) { // nếu mở thành công thì thực hiện
						fseek(f, 0, SEEK_END);
						long filesize = ftell(f); // kích thước file
						char * data = (char*)calloc(filesize, 1);
						fseek(f, 0, SEEK_SET);
						fread(data, 1, filesize, f); // đọc 1 lần hết dữ liệu
						fclose(f);
						// gửi mã 150
						char sbuf[1024] = "150 Opening data channel\r\n";
						send(c, sbuf, strlen(sbuf), 0);
						SOCKADDR_IN caddr_t;
						int clen_t = sizeof(caddr_t);
						// chấp nhận kết nối đến socket sdata vừa tạo ở trên lệnh PASV, được socket sd1 và gửi dữ liệu đi rồi đóng kết nối trên socket này
						SOCKET sd1 = accept(sdata, (sockaddr*)&caddr_t, &clen_t);
						send(sd1, data, filesize, 0);
						closesocket(sd1); // đóng kết nối
						// gửi mã báo thành công
						strcpy(sbuf, "226 Successful transfering\r\n");
						send(c, sbuf, strlen(sbuf), 0);
						trans_mode = 0;
						free(data); // giải phóng bộ nhớ
					}
					else { // f=NULL <=> lỗi
						char sbuf[] = "550 Cannot open file.\r\n";
						send(c, sbuf, strlen(sbuf), 0);
					}
				}
				else {
					char sbuf[] = "550 File not found.\r\n";
					send(c, sbuf, strlen(sbuf), 0);
				}
			}
		}

		// LỆNH STORE : TẢI FILE LÊN SERVER
		else if (strcmp(cmd, "STOR") == 0) { // tải file lên server
			if (!login) { // yêu cầu login 
				char sbuf[] = "530 Please login with USER and PASS!\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else {
				char v_path[PATH_LENGTH] = "";
				char p_path[PATH_LENGTH] = "";
				processPathname(cwd, user->h_path, args, v_path, p_path);
				// lấy tên file và đường dẫn tới thư mục chứa file.
				char dpath[2 * PATH_LENGTH] = ""; // đường dẫn tới thư mục chứa file
				char filename[PATH_LENGTH] = ""; // tên file
				strcpy(dpath, p_path);
				char * last = strrchr(dpath, '/');
				strcpy(filename, last + 1);
				*last = '\0';
				if (findFile(dpath) == 1 && findFile(p_path) == 0) {
					// findFile(dpath) == 1: tìm thấy thư mục chứa file này
					// findFile(p_path) == 0: ko tìm thấy file -> có thể tạo file mới và lưu trữ ở đây
					FILE * f = fopen(p_path, "wb");
					if (f != NULL) {
						char sbuf[1024] = "";
						sprintf(sbuf, "150 Opening data channel for file upload to server of \"%s\"\r\n", args);
						send(c, sbuf, strlen(sbuf), 0);
						// chấp nhận kết nối đến socket sdata vừa tạo ở trên lệnh PASV, được socket sd1 và gửi dữ liệu đi rồi đóng kết nối trên socket này
						SOCKADDR_IN caddr_t;
						int clen_t = sizeof(caddr_t);
						SOCKET sd1 = accept(sdata, (sockaddr*)&caddr_t, &clen_t);
						// nhận dữ liệu và ghi vào file
						while (true) {
							char dbuf[1024];
							memset(dbuf, 0, sizeof(dbuf));
							int size = recv(sd1, dbuf, sizeof(dbuf) - 1, 0); // nhận dữ liệu
							if (size > 0) fwrite(dbuf, 1, size, f); // ghi dạng binary vào file
							else break; // không nhận được byte nào (phía client đóng kết nối) thì thoát.
						}
						closesocket(sd1); // đóng kết nối
						// gửi mã báo thành công
						strcpy(sbuf, "226 Successful transfering.\r\n");
						send(c, sbuf, strlen(sbuf), 0);
						trans_mode = 0;
						fclose(f);
					}
					else { // không mở được file để ghi
						char sbuf[] = "550 Cannot create file.\r\n";
						send(c, sbuf, strlen(sbuf), 0);
					}
				}
				else {
					char sbuf[] = "550 Filename invalid.\r\n";
					send(c, sbuf, strlen(sbuf), 0);
				}
			}
		}

		//LỆNH SYSTEM: THÔNG TIN HỆ THỐNG CỦA SERVER
		else if (strcmp(cmd, "SYST") == 0) { // thông tin về hệ thống
			char sbuf[] = "215 UNIX\r\n";
			send(c, sbuf, strlen(sbuf), 0);
		}

		//LỆNH SIZE: SIZE CỦA FILE
		else if (strcmp(cmd, "SIZE") == 0) { // size của file, size của thư mục thì báo lỗi	
			if (!login) {
				// yêu cầu login 
				char sbuf[] = "530 Please login with USER and PASS!\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else {
				char v_path[PATH_LENGTH] = "";
				char p_path[PATH_LENGTH] = "";
				processPathname(cwd, user->h_path, args, v_path, p_path);
				int size = sizeOfFile(p_path);
				if (size != 0) {
					char sbuf[1024] = "";
					sprintf(sbuf, "%d %d\r\n", 213, size);
					send(c, sbuf, strlen(sbuf), 0);
				}
				else {
					char sbuf[] = "550 File not found.\r\n";
					send(c, sbuf, strlen(sbuf), 0);
				}
			}
		}

		//LỆNH TYPE: kiểu dữ liệu biểu diễn
		else if (strcmp(cmd, "TYPE") == 0) { // kiểu dữ liệu biểu diễn
			if (!login) {
				// yêu cầu login 
				char sbuf[] = "530 Please login with USER and PASS!\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else if (strcmp(args, "I") == 0 || strcmp(args, "A") == 0) { // kiểu nhị phân hoặc ascii
				char sbuf[] = "200 OK.\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
			else {
				// lỗi tham số
				char sbuf[] = "501 Syntax error.\r\n";
				send(c, sbuf, strlen(sbuf), 0);
			}
		}
		else if (strcmp(cmd, "QUIT") == 0) { // kết thúc phiên
			char sbuf[] = "221 Goodbye.\r\n";
			send(c, sbuf, strlen(sbuf), 0);
			break;

		}
		else {	// lệnh không được xử lý sẽ coi là lỗi cú pháp
			char sbuf[] = "500 Systax error, command unrecognized.\r\n";
			send(c, sbuf, strlen(sbuf), 0);
		}
	}
	return 0;
}

int main() {
	// khoi tao wsadata
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
		cout << "Loi khi khoi tao WSA" << endl;
		system("pause");
		return 1;
	}
	// khởi tạo socket và bind, lắng nghe trên cổng 21.
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SOCKADDR_IN saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(21);
	saddr.sin_addr.s_addr = 0;
	if (bind(s, (sockaddr*)&saddr, sizeof(saddr)) != 0) {
		cout << "Loi khi bind socket" << endl;
		system("pause");
		return 1;
	}
	if (listen(s, 10) != 0) {
		cout << "Loi khi listen" << endl;
		system("pause");
		return 1;
	}
	readConfigFile(); // doc file config
	while (true) {
		SOCKADDR_IN caddr;
		int clen = sizeof(caddr);
		// chấp nhạn client kết nối đến và tạo thread đối với client.
		SOCKET c = accept(s, (sockaddr*)&caddr, &clen);
		cnum++;
		CreateThread(NULL, 0, clientThread, (LPVOID)c, 0, NULL);
	}
	clear();
	system("pause");
	return 0;
}
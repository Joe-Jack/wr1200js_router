#include <iostream>
#include <string>
#include <vector>
#include <openssl/evp.h>
#include <memory>
#include "tinyjson.hpp"
#include <curl/curl.h>
#include <fstream>

using namespace tiny;

// 全局函数
std::vector<std::string> splitString(const std::string& str, const std::string& delimiter)
{
	std::vector<std::string> tokens;

	// str为空字符串
	if (str == "") {
		return tokens;
	}

	// delimiter为空字符串
	if (delimiter == "") {
		tokens.push_back(str);
		return tokens;
	}

	// 字符串分割
	size_t startPos = 0;
	auto index = str.find_first_of(delimiter);
	while (index != std::string::npos) {
		auto temp = str.substr(startPos, index - startPos);
		if (temp != "") {
			tokens.push_back(temp);
		}
		startPos = index + 1;
		index = str.find_first_of(delimiter, startPos);
	}

	// 最后一个字符串
	auto temp = str.substr(startPos);
	if (temp != "") {
		tokens.push_back(temp);
	}

	return tokens;
}

class Protocol {
public:
	std::string protocol;
	std::string name;   // 名字
	std::string server_name;  // 服务器
	unsigned int port;   // 端口
	std::string password;  // 密码
	// 成员函数
	static bool base64Decode(const std::string& encoded, std::string& decoded) {
		size_t src_len = encoded.size(), decode_len;
		int ret, i = 0;

		if (src_len % 4 != 0) return false;
		decode_len = (src_len / 4) * 3;
		decoded.resize(decode_len);

		ret = EVP_DecodeBlock((unsigned char*)decoded.c_str(), (const unsigned char*)encoded.c_str(), (int)src_len);
		if (ret == -1) {
			decoded = encoded;
			return false;
		}

		while (encoded.at(--src_len) == '=') {
			ret--;
			if (++i > 2) return false;
		}
		decoded.resize(ret);
		return true;
	}

	virtual TinyJson json_output() {
		TinyJson example;
		return example;
	}

	static std::string urlDecode(std::string &eString) {
		std::string ret;
		char ch;
		unsigned int i, j;
		for (i=0; i<eString.length(); i++) {
			if (int(eString[i])==37) {
				sscanf(eString.substr(i+1,2).c_str(), "%x", &j);
				ch=static_cast<char>(j);
				ret+=ch;
				i=i+2;
			} else {
				ret+=eString[i];
			}
		}
		return (ret);
	}

	static std::string Base64Encode(const unsigned char* data, size_t size)
	{
		size_t base64_len = (size + 2) / 3 * 4;
		if (base64_len == 0)
		{
			return "";
		}
		std::string ret;
		ret.resize(base64_len);
		EVP_EncodeBlock((unsigned char*)ret.data(), data, size);
		return std::move(ret);
	}
};

class Shadowsocks: public Protocol {
private:
	static int count;

	void parseData(const std::string& data) {
		std::vector<std::string> param, child;
		std::string delimiter = ":@#", decoded_param;  //特有分割符
		char terminator = '=';
		int padding_num = 0;
		param = splitString(data, delimiter);

		//解析ss://method:password@server:port
		param[1].erase(0, 2); // 删除'//' 符号
		padding_num = 4 - (param[1].size() % 4);

		if (padding_num) {
			param[1].append(padding_num, terminator);
		}

		if (base64Decode(param[1], decoded_param)) {
			param.erase(param.begin() + 1);
			child = splitString(decoded_param, delimiter);
			param.insert(param.begin() + 1, child.begin(), child.end());
			if (child.size() > 2) { // 支持2022协议
				param.erase(param.begin() + 3);
				param[2] += ":" + child[2];
			}
		} else if (padding_num) {
			param[1].erase(param[1].end() - padding_num);
		}

		//初始化赋值
		protocol = param[0];
		method = param[1];
		password = param[2];
		server_name = param[3];
		port = std::stoi(param[4]);
		name = param[5];
		return;
	}
public:
	std::string method;

	Shadowsocks(const std::string& ss) {
		++count;
		parseData(ss);
	}

	Shadowsocks(const std::string& a, const std::string& b, unsigned int c, \
			const std::string& d, const std::string& e) {
		++count;
		name = a;
		server_name = b;
		port = c;
		password = d;
		protocol = "ss";
		method = e;
	}

	TinyJson json_output() override {
		TinyJson ss_json;
		ss_json["proto"].Set(protocol);
		ss_json["server"].Set(server_name);
		ss_json["port"].Set(port);
		ss_json["method"].Set(method);
		ss_json["password"].Set(password);
		ss_json["name"].Set(Base64Encode((const unsigned char*)name.c_str(), (int)name.size()));
		return ss_json; 
	}

	static int getCount() {
		return count;
	}
};

// 初始化静态成员变量
int Shadowsocks::count = 0;

std::unique_ptr<Protocol> createProtocol(const std::string& type, const std::string& ss_proto) {
	if (type == "ss") {
		return std::make_unique<Shadowsocks>(ss_proto);
	} else {
		return nullptr;
	}
}

size_t req_reply(void *ptr, size_t size, size_t nmemb, void *stream)
{
    std::string *str = (std::string*)stream;
    //std::cout << *str << std::endl;
    (*str).append((char*)ptr, size*nmemb);
    return size * nmemb;
}

CURLcode curl_get_req(const std::string &url, std::string &response)
{
    // init curl
    CURL *curl = curl_easy_init();
    // res code
    CURLcode res = CURLE_OK;
    if (curl)
    {
        // set params
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); // url
       	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // if want to use https
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false); // set peer and host verify false
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, req_reply);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3); // set transport and time out time
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
        // start req
        res = curl_easy_perform(curl);
    }
    // release curl
    curl_easy_cleanup(curl);
    return res;
}

int main(int argc, char *argv[]) {
	std::string  decoded, encoded, input;
	std::string str, del = "\n", ss_file = "/tmp/ss_link"; // 输出字符串
	std::vector<std::string> tokens; // 逐行输入
	std::vector<std::unique_ptr<Protocol>> protos;
	TinyJson jsonarray;
	std::fstream f;
	
	if (argc < 2) {
		std::cout << "输入url" << std::endl;
		return 1;
	}

	input = argv[1];
	curl_global_init(CURL_GLOBAL_ALL);
	if (input.find("http://") == 0 || input.find("https://") == 0) {
		if (curl_get_req(input, encoded) != CURLE_OK) {
			return 2;
		}
	} else {
		std::cerr << "无效的 URL: " << input << "\n";
		return 1;
	}

	if (Protocol::base64Decode(encoded, decoded)) {
		tokens = splitString(decoded, del);
		for (auto it = tokens.begin(); it != tokens.end(); ++it) {
			char targetChar = ':'; //协议分割符
			size_t pos = (*it).find(targetChar);
			if (pos == std::string::npos)  continue;
			auto proto = createProtocol((*it).substr(0, pos), *it);
			if (proto == nullptr) continue;
			protos.push_back(std::move(proto));
		}
		for (const auto& test : protos) {
			jsonarray.Push(test->json_output());
		}
		std::string str = jsonarray.WriteJson(2);
		f.open(ss_file,std::ios::out);
		f << str << std::endl;
		std::cout << str << std::endl; // 输出解码后的字符串
    }
    return 0;
}


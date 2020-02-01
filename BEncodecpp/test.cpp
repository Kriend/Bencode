#include "bencode.h"
#include <iostream>
#include <vector>
#include <cmath>
using namespace std;

#define TORRENT_FILE_NAME "0B35A9A31C2AF8A05183A3FAF210CE9CAB7898F3.torrent"

int main() {
	FILE* fp = NULL;
	fopen_s(&fp, TORRENT_FILE_NAME, "rb"); 
	if (!fp)
	{
		cout << "文件打开失败" << endl;
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	int sum;
	vector<char> bt;
	bt.resize(size);
	sum = fread(&bt[0], 1, size, fp);

	cout << "File Size: " << sum << endl;
	Bencode::Value root;
	Bencode::Reader reader;
	if (reader.parse(&bt[0], &bt[0]+sum, root)) cout << "Parse OK!" << endl;
	long long filesize = 0;
	bool isMultifile = false;
	
	isMultifile = root["info"].isMember("files");

	if (isMultifile)
	{
		cout << "MultiFile" << endl;
		for (auto file : root["info"]["files"])
		{
			filesize += file["length"].asInt();
		}
	}
	else
	{
		cout << "Single File" << endl;
		filesize = root["info"]["length"].asInt();
	}
	


	int piece_length = root["info"]["piece length"].asInt();
	int block = ceil((double)filesize / piece_length);
	int sha1_length = block * 20;
	cout << "filesize: " << filesize << endl;
	cout << "sha1_length: " << sha1_length << endl;

	//Bencode::Writer writer;
	//unsigned int length = writer.write(root);
	//cout << "Writer Length: " << length << endl;
	//fopen_s(&fp, "writer.torrent", "wb");
	//if (!fp)
	//{
	//	cout << "文件打开失败" << endl;
	//	return 0;
	//}
	//fwrite(writer.getCString(), 1, length, fp);
	//fclose(fp);

	system("pause");
	return 0;
}

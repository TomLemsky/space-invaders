#include "Machine.h"
#include <string>
#include <iostream>
#include <memory>

using namespace std;

int main(int argc, char *argv[]){
    string game_name = "invaders.";
    unique_ptr<Machine> machine;

    // Does .e file exist?
    FILE * fp = fopen((game_name+"e").c_str(), "rb");
    if(fp!=NULL){
        fclose(fp);
        machine = make_unique<Machine>(game_name);
    } else {
        machine = make_unique<Machine>("invaders.bin");
    }
    machine->run();
}

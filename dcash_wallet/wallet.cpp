#define RAPIDJSON_HAS_STDSTRING 1

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "WwwFormEncodedDict.h"
#include "HttpClient.h"

#include "rapidjson/document.h"

using namespace std;
using namespace rapidjson;

int API_SERVER_PORT = 8080;
string API_SERVER_HOST = "localhost";
string PUBLISHABLE_KEY = "";

string auth_token;
string user_id;

vector<string > parse_data(string input); // function that will parse the data and put it into a vector
// function that will handle commands of the user or file
void command_handler(vector<string> parseCom, bool& auth);
void balance(); // handles the balance command
void auth_fn(vector<string> parseCom); // handles the auth command
void logout(); // deletes auth_token
void send(vector<string> parseCom); // transfer money from one user to another
void deposit(vector<string> parseCom); // deposit money into an account


int main(int argc, char *argv[]) {
  stringstream config;
  int fd = open("config.json", O_RDONLY);
  if (fd < 0) {
    cout << "could not open config.json" << endl;
    exit(1);
  }
  int ret;
  char buffer[4096];
  while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
    config << string(buffer, ret);
  }
  Document d;
  d.Parse(config.str());
  API_SERVER_PORT = d["api_server_port"].GetInt();
  API_SERVER_HOST = d["api_server_host"].GetString();
  PUBLISHABLE_KEY = d["stripe_publishable_key"].GetString();

  // declare variables
  string command; // string that will be used to store the unparsed input from user or file
  vector<string> parseCom; // will be used to store the parsed commands from user or file
  ifstream file; // used in batch mode to open files and read input
  bool auth = false;
  //cout << setprecision(2) << fixed; // only 2 decimal points will be printed

  // start the while loop
  while(command != "exit" || file.eof()){ // runs until the user types exit or reach end of file
      if(argc == 1){ // interactive mode, has the prompt D$> and use getline()
          cout << "D$> ";
          getline(cin, command);
          // check if exit was typed
          if(command == "exit"){
              exit(0);
          }
          else{ // command was not exit so parse the input
              parseCom = parse_data(command); // parse the data
              // run command_handler
              command_handler(parseCom, auth);
              if(parseCom[0] == "logout"){
                  exit(0);
              }
          }
      }
      else{ // batch mode, read from file and use getline()
          // open file from the argument
          file.open(argv[1]);
          // check if the file was opened or is empty. if it is either send error message
          if(file.fail() || file.peek() == std::ifstream::traits_type::eof()){
              // exit program with 1
              //cout << "Bad file test" << endl;
              exit(1);
          }
          else{ // file is good and now read line by line
              while(getline(file, command) || file.eof()){
                  // check if exit was the next command or end of file was reached
                  if(command == "exit" || file.eof()){
                      exit(0);
                  }
                  else{ // command was not exit so parse the input
                      parseCom = parse_data(command); // parse the data
                      // run command_handler
                      command_handler(parseCom, auth);
                      if(parseCom[0] == "logout"){
                          exit(0);
                      }
                  }
                  // clear vector for next line
                  parseCom.clear();
              }
              file.close(); // close the file
          }
      }
  }
  return 0;
}

// function that will parse the data and put it into a vector
vector<string > parse_data(string input){
    vector<string > string_parsed;
    string word;
    stringstream ss;

    if(input.empty()){
        string_parsed.push_back("Bad");
    }
    else{
        ss << input;
        while(getline(ss,word,' ')){
            string_parsed.push_back(word);
        }
    }
    return string_parsed;
}

// function that will handle commands of the user or file
void command_handler(vector<string> parseCom, bool& auth){
    string com = parseCom[0]; // this will be the command
    // if statements for which command to run
    if(com == "auth"){
        // only one where user does not have to be logged in
        // check that there are correct number of arguments
        if(!(parseCom.size() == 4 || parseCom.size() == 3)){
            cout << "Error" << endl;
        }
        else{
            // if successful log in, make auth true
            auth = true;
            // save auth_token and userid
            auth_fn(parseCom);
        }
    }
    else if(com == "balance" && auth){
        // check that there are correct number of arguments
        if(parseCom.size() != 1){
            cout << "Error" << endl;
        }
        else{
            // call the balance function
            balance();
        }
    }
    else if(com == "deposit"  && auth){
        // check that there are correct number of arguments
        if(parseCom.size() != 6){
            cout << "Error" << endl;
        }
        else{
            deposit(parseCom);
        }
    }
    else if(com == "send"  && auth){
        // check that there are correct number of arguments
        if(parseCom.size() != 3){
            cout << "Error" << endl;
        }
        else{
            send(parseCom);
        }
    }
    else if(com == "logout" && auth){
        // check that there are correct number of arguments
        if(parseCom.size() != 1){
            cout << "Error" << endl;
        }
        else{
            // delete auth_token and make auth to false
            auth = false;
            logout();
        }
    }
    else{ // no matching commands means invalid so print Error and start again
        cout << "Error" << endl;
    }
}

// handles the balance command
void balance(){
    string encoded_body = "/users/" + user_id;

    // create the request to gunrock using the HTTPClient object
    HttpClient gun_rock_request(API_SERVER_HOST.c_str(),API_SERVER_PORT,false);
    gun_rock_request.set_header("x-auth-token",auth_token);
    // interpret response from gunrock
    HTTPClientResponse * gun_rock_response = gun_rock_request.get(encoded_body);

    // check if gunrock sent back an error
    if(!gun_rock_response->success()){
        cout << "Error" << endl;
    }
    else{
        // convert the HTTP body into RapidJSON document
        Document* d = gun_rock_response->jsonBody();
        // extract the balance
        int balance = (*d)["balance"].GetInt();
        double bal = balance / 100.0;
        cout << "Balance: $" << bal <<  ".00" << endl; //<< setprecision(2) << fixed
    }
}

// handles the auth command
void auth_fn(vector<string> parseCom){
    // post has path and body
    WwwFormEncodedDict body; // to encode and for API call to gunrock
    string username = parseCom[1];
    string password = parseCom[2];

    // if there is another user, call logout
    if(!auth_token.empty()){
        // call logout to delete auth_token
        logout();
    }
    string path = "/auth-tokens"; // this is the path for the HTTPClient
    body.set("username", username);
    body.set("password",password);
    string encoded_body = body.encode();

    // create the request to gunrock using the HTTPClient object
    HttpClient gun_rock_request(API_SERVER_HOST.c_str(),API_SERVER_PORT,false);
    // interpret response from gunrock
    HTTPClientResponse * gun_rock_response = gun_rock_request.post(path,encoded_body);

    // check if gunrock sent back an error
    if(!gun_rock_response->success()){
        cout << "Error" << endl;
    }
    else{
        // convert the HTTP body into RapidJSON document
        Document* d = gun_rock_response->jsonBody();
        // get the auth_token and userid
        auth_token = (*d)["auth_token"].GetString();
        user_id = (*d)["user_id"].GetString();

        // if there is an email, update using PUT
        if(parseCom.size() == 4){
            string email = parseCom[3];
            WwwFormEncodedDict newBody;
            path = "/users/" + user_id;
            newBody.set("email",email);
            encoded_body = newBody.encode();

            // create the request to gunrock using the HTTPClient object
            HttpClient new_request(API_SERVER_HOST.c_str(),API_SERVER_PORT,false);
            new_request.set_header("x-auth-token",auth_token);

            gun_rock_response = new_request.put(path,encoded_body);
            if(!gun_rock_response->success()){
                cout << "Error" << endl;
                return;
            }
        }
        // call balance to print balance
        balance();
    }
}

// deletes auth_token
void logout(){
    string path = "/auth-tokens/" + auth_token;

    // create the request to gunrock using the HTTPClient object
    HttpClient gun_rock_request(API_SERVER_HOST.c_str(),API_SERVER_PORT,false);
    gun_rock_request.set_header("x-auth-token",auth_token);
    // interpret response from gunrock
    HTTPClientResponse * gun_rock_response = gun_rock_request.del(path);

    // check if gunrock sent back an error
    if(!gun_rock_response->success()){
        cout << "Error" << endl;
    }
    else{
        // make auth_token to empty string
        auth_token = "";
    }
}

// transfer money from one user to another
void send(vector<string> parseCom){
    string to = parseCom[1];
    double cents = stod(parseCom[2]) * 100;
    WwwFormEncodedDict body; // to encode and for API call to gunrock

    string path = "/transfers"; // this is the path for the HTTPClient
    body.set("to", to);
    body.set("amount",to_string(cents));
    string encoded_body = body.encode();

    // create the request to gunrock using the HTTPClient object
    HttpClient gun_rock_request(API_SERVER_HOST.c_str(),API_SERVER_PORT,false);
    gun_rock_request.set_header("x-auth-token",auth_token);
    // interpret response from gunrock
    HTTPClientResponse * gun_rock_response = gun_rock_request.post(path,encoded_body);

    // check if gunrock sent back an error
    if(!gun_rock_response->success()){
        cout << "Error" << endl;
    }
    else{
        balance();
    }
}

// deposit money into an account
void deposit(vector<string> parseCom){
    // set the credit card info
    double amount = stod(parseCom[1])*100.0;
    string card_num = parseCom[2];
    string exp_yr = parseCom[3];
    string exp_m = parseCom[4];
    string cvc = parseCom[5];

    // the body for the stripe call for credit card token
    WwwFormEncodedDict stripe;
    string stripe_path = "/v1/tokens";
    // set the values for the stripe request
    stripe.set("card[number]",card_num);
    stripe.set("card[exp_month]", exp_m);
    stripe.set("card[exp_year]",exp_yr);
    stripe.set("card[cvc]", cvc);
    // encode the stripe body for request
    string stripe_body = stripe.encode();

    // create the request to stripe using the HTTPClient object
    HttpClient stripe_request("api.stripe.com",443,true);
    stripe_request.set_basic_auth(PUBLISHABLE_KEY,"");

    // interpret the result from Stripe
    HTTPClientResponse *stripe_response = stripe_request.post(stripe_path,stripe_body);

    // check if the response sent an error
    if(!stripe_response->success()){
        cout << "Error" << endl;
    }
    else{
        // convert the HTTP body into a RapidJSON document
        Document *d = stripe_response->jsonBody();
        // extract the charge id and amount
        string token = (*d)["id"].GetString(); // id is key to token might be object idk
        delete d;

        // charge credit card through gunrock server
        WwwFormEncodedDict gunrock;
        string gunrock_path = "/deposits";

        gunrock.set("amount",to_string(amount));
        gunrock.set("stripe_token", token);
        string gunrock_body = gunrock.encode();

        // create the request to gunrock using the HTTPClient object
        HttpClient gunrock_request(API_SERVER_HOST.c_str(),API_SERVER_PORT,false);
        gunrock_request.set_header("x-auth-token",auth_token);
        // interpret response from gunrock
        HTTPClientResponse * gunrock_response = gunrock_request.post(gunrock_path,gunrock_body);

        // check if gunrock sent back an error
        if(!gunrock_response->success()){
            cout << "Error" << endl;
        }
        else{
            balance();
        }
    }
}

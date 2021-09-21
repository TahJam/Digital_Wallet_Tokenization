#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AuthService.h"
#include "StringUtils.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

AuthService::AuthService() : HttpService("/auth-tokens") {
  
}

/** if username doesn't exist, this function will be called to create new User
 * if a user does exist, then log into the user if the password matches
 * username must be all lowercase
 *
 * @param request
 * @param response -> must set response to the stuff in api_server.md
 */
void AuthService::post(HTTPRequest *request, HTTPResponse *response) {
    // get the username from request
    WwwFormEncodedDict stuff = request->formEncodedBody();
    string username = stuff.get("username");
    string password = stuff.get("password");

    // check if username and password have stuff in it
    if(username.empty() || password.empty()){
        throw ClientError::notFound();
    }

    // check if username is in all lowercase
    for(unsigned int i = 0; i < username.size();i++){
        if(!isalpha(username[i]) || !islower(username[i]) ){ // character is not alpha or lowercase
            throw ClientError:: badRequest();
        }
    }

    //check if username exists in the database
    if(m_db->users.count(username) > 0){
        // if it does, then check password
        User* user = m_db->users[username];
        if(password == user->password){
            // create auth_token using StringUtils and get the userid from User
            string auth_token = StringUtils::createAuthToken();
            string my_user_id = user->user_id;
            // insert auth_token to the database
            m_db->auth_tokens.insert(pair<string, User*>(auth_token, user));

            // use the RapidJSON thing
            Document document;
            Document::AllocatorType& a = document.GetAllocator();
            Value o;
            o.SetObject();
            o.AddMember("auth_token", auth_token, a);
            o.AddMember("user_id", my_user_id, a);
            document.Swap(o);
            StringBuffer buffer;
            PrettyWriter<StringBuffer> writer(buffer);
            document.Accept(writer);
            response->setContentType("application/json");
            response->setBody(buffer.GetString() + string("\n"));
            response->setStatus(200);
        }
        else{
            // password was incorrect
            throw ClientError::unauthorized();
        }
    }
    else{ // if username does not exist, create new User object with that username
        // create unique userid and auth_token
        string userid = StringUtils::createUserId();
        string auth_token = StringUtils::createAuthToken();
        // create new User obj
        User* newUser = new User();
        newUser -> username = username;
        newUser -> password = password;
        newUser -> user_id = userid;
        newUser->balance = 0;

        // add the username to the map
        m_db->users.insert(pair<string, User*>(username,newUser));
        // add the auth_token to the map
        m_db->auth_tokens.insert(pair<string, User*>(auth_token, newUser));

        // use the RapidJSON thing
        Document document;
        Document::AllocatorType& a = document.GetAllocator();
        Value o;
        o.SetObject();
        o.AddMember("auth_token", auth_token, a);
        o.AddMember("user_id", userid, a);
        document.Swap(o);
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        document.Accept(writer);
        response->setContentType("application/json");
        response->setBody(buffer.GetString() + string("\n"));
        response->setStatus(201);
    }
}

void AuthService::del(HTTPRequest *request, HTTPResponse *response) {
    // parse the request to get the stuff
    vector<string> path = request->getPathComponents();
    string auth_token = path[path.size()-1];
    // check if the user matches
    User* user = getAuthenticatedUser(request);
    if (user != m_db->auth_tokens[auth_token]){
        // usernames don't match so throw error
        throw ClientError::unauthorized();
    }
    else{
        // remove the path from the auth_token map
        std::map<string,User*>::iterator it = m_db->auth_tokens.find(auth_token);
        m_db->auth_tokens.erase(it);
    }
}

#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AccountService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

AccountService::AccountService() : HttpService("/users") {
  
}

/**
 * fetch the User object for this account. Can only fetch the User object for the account
 * that is authorized
 *
 * @param request
 * @param response
 */
void AccountService::get(HTTPRequest *request, HTTPResponse *response) {
    // get the auth_token from the request
    string token = request ->getAuthToken();
    // get the userid from the request
    vector<string> things = request->getPathComponents();
    string req_userid = things[things.size()-1];
    // get the User from the request
    User* reqUser = getAuthenticatedUser(request);
    // check if the auth_token maps to the same User as the request and the userid match
    if(reqUser != m_db->auth_tokens[token] || req_userid != m_db->auth_tokens[token]->user_id){
        // if it doesn't, throw error
        throw ClientError::unauthorized();
    }
    else{
        // do the RapidJSON thing and return the email and balance (in cents)
        Document document;
        Document::AllocatorType& a = document.GetAllocator();
        Value o;
        o.SetObject();
        o.AddMember("email", reqUser->email, a);
        o.AddMember("balance", reqUser->balance, a);
        document.Swap(o);
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        document.Accept(writer);
        response->setContentType("application/json");
        response->setBody(buffer.GetString() + string("\n"));
        response->setStatus(200);
    }
}

/**
 * Updates the info for a User. email
 *
 * @param request  -> contains the email for the User
 * @param response -> email and balance on the account in cents
 */
void AccountService::put(HTTPRequest *request, HTTPResponse *response) {
    // transform the request into the dict
    WwwFormEncodedDict stuff = request->formEncodedBody();
    // get the userid from the request
    vector<string> things = request->getPathComponents();
    string req_userid = things[things.size()-1];

    // get the email from the request
    string usr_email = stuff.get("email");
    // get the auth_token
    string token = request->getAuthToken();
    // get the User from the request
    User* reqUser = getAuthenticatedUser(request);
    // check if the email is valid
    if (usr_email.empty()){
        throw ClientError:: notFound();
    }
    // check if the tokens match and the userid match
    else if (reqUser != m_db->auth_tokens[token] || req_userid != m_db->auth_tokens[token]->user_id){
        throw ClientError::unauthorized();
    }
    else{
        // update the email for the user
        reqUser->email = usr_email;
        // do the RapidJSON thing
        Document document;
        Document::AllocatorType& a = document.GetAllocator();
        Value o;
        o.SetObject();
        o.AddMember("email", reqUser->email, a);
        o.AddMember("balance", reqUser->balance, a);
        document.Swap(o);
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        document.Accept(writer);
        response->setContentType("application/json");
        response->setBody(buffer.GetString() + string("\n"));
        response->setStatus(200);
    }
}

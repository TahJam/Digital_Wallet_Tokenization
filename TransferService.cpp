#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "TransferService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

TransferService::TransferService() : HttpService("/transfers") { }


void TransferService::post(HTTPRequest *request, HTTPResponse *response) {
    // get the from User
    User* from = getAuthenticatedUser(request);
    WwwFormEncodedDict stuff = request->formEncodedBody(); // get stuff from request to get amount and stripe_token
    WwwFormEncodedDict body; // to encode and for API call to stripe
    // get the amount that needs to be transferred
    string str_amount = stuff.get("amount");
    int amount = stoi(str_amount);
    // get the username from stuff
    string user_to = stuff.get("to");

    // if the string is empty then that means nothing was returned and so throw error
    if (user_to.empty()){
        throw ClientError::badRequest();
    }
        // if the to User object is not found, throw error
    else if(m_db->users.find(user_to) == m_db->users.end()){
        throw ClientError::notFound();
    }
    else if(amount < 0 || amount > from->balance){ // check if the amount is negative or amount is greater that from's
                                                    // balance
        throw ClientError:: badRequest();
    }
    else{ // everything is good so now transfer funds
        User* to = m_db->users[user_to];

        // subtract the balance from the from User and add it to the to User
        from->balance -=amount;
        to->balance+=amount;

        // add the transfer to the transfer vector
        Transfer* trans = new Transfer;
        trans->from = from;
        trans->to = to;
        trans->amount = amount;
        m_db->transfers.push_back(trans);

        // do the RapidJSON thing
        Document document;
        Document::AllocatorType& a = document.GetAllocator();
        Value o;
        o.SetObject();
        // create array for RapidJSON
        Value res_array;
        res_array.SetArray();
        // loop through m_db.transfers vector to add all the transfers to the array
        std::vector<Transfer *> db = m_db->transfers;
        for(unsigned int i =0; i < db.size(); i++){
            if(from->username == db[i]->from->username){
                // add the from, to and amount to object
                Value trans_stuff;
                trans_stuff.SetObject();
                // add from's username
                trans_stuff.AddMember("from",db[i]->from->username,a);
                // add to's username
                trans_stuff.AddMember("to",db[i]->to->username,a);
                // add the amount
                trans_stuff.AddMember("amount", amount,a);
                // push object to array
                res_array.PushBack(trans_stuff,a);
            }
        }
        o.AddMember("balance", from->balance, a);
        o.AddMember("transfers", res_array, a);
        document.Swap(o);
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        document.Accept(writer);
        response->setContentType("application/json");
        response->setBody(buffer.GetString() + string("\n"));
        response->setStatus(200);
    }
}

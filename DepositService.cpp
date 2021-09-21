#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "DepositService.h"
#include "Database.h"
#include "ClientError.h"
#include "HTTPClientResponse.h"
#include "HttpClient.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

DepositService::DepositService() : HttpService("/deposits") { }

/**
 * Uses a credit card to deposit money into account.
 *
 * @param request
 * @param response
 */
void DepositService::post(HTTPRequest *request, HTTPResponse *response) {
    // get the auth_token for accessing User object
    string auth_token = request->getAuthToken();
    User* to = getAuthenticatedUser(request);
    WwwFormEncodedDict stuff = request->formEncodedBody(); // get stuff from request to get amount and stripe_token
    WwwFormEncodedDict body; // to encode and for API call to stripe
    // get the amount that needs to be transferred
    string str_amount = stuff.get("amount");
    int amount = stoi(str_amount);
    // check that amount is non-negative
    if (amount < 50){
        throw ClientError:: badRequest();
    }
    // get the stripe token from stuff
    string stripe_token = stuff.get("stripe_token");
    // check if stripe_token is valid
    if (stripe_token.empty()){
        //cout << "in the stripe_token" << endl;
        throw ClientError::badRequest();
    }

    // create the body and get it ready for HTTPClient
    body.set("amount", amount);
    body.set("currency", "usd");
    body.set("source",stripe_token);

    string encoded_body = body.encode();

    // create the request to stripe using the HTTPClient object
    HttpClient server_request("api.stripe.com",443,true);
    server_request.set_basic_auth(m_db->stripe_secret_key,"");

    // interpret the result from Stripe
    HTTPClientResponse *stripe_response = server_request.post("/v1/charges",encoded_body);

    // check if stripe sent back an error
    if (!stripe_response->success()){
        cout << "in the success" << endl;
        throw ClientError:: badRequest();
    }

    // convert the HTTP body into a RapidJSON document
    Document *d = stripe_response->jsonBody();
    // extract the charge id and amount
    string charge_id = (*d)["id"].GetString(); // id and amount are keys to values
    int stripe_amount = (*d)["amount"].GetInt();
    delete d;

    // add the charge_id and stripe_amount to deposit vector
    Deposit* from_stripe = new Deposit;
    from_stripe->amount = stripe_amount;
    from_stripe->to = to;
    from_stripe->stripe_charge_id = charge_id;

    // insert from_stripe into the m_db
    m_db->deposits.push_back(from_stripe);

    // add the amount to the user's balance
    to->balance += stripe_amount;

    // use the RapidJSON thing
    Document document;
    Document::AllocatorType& a = document.GetAllocator();
    Value o;
    o.SetObject();

    // create array for RapidJSON
    Value res_array;
    res_array.SetArray();

    // loop through the m_db.deposits vector to add all the deposits to array
    std::vector<Deposit *> db = m_db->deposits;
    for(unsigned int i =0; i < db.size();i++){
        if (to->user_id == db[i]->to->user_id){
            // add the deposit stuff to the res_array
            Value user_name;
            // add the username
            user_name.SetObject();
            // add username
            user_name.AddMember("to",db[i]->to->username,a);
            // add amount
            user_name.AddMember("amount",db[i]->amount,a);
            // add stripe_charge_id
            user_name.AddMember("stripe_charge_id",db[i]->stripe_charge_id,a);
            // push object into array
            res_array.PushBack(user_name, a);
        }
    }

    o.AddMember("balance", to->balance, a);
    o.AddMember("deposits", res_array, a);
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
    response->setStatus(200);
}

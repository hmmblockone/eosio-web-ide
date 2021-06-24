#include <eosio/eosio.hpp>

// Message table
struct [[eosio::table("message"), eosio::contract("talk")]] message {
    uint64_t    id       = {}; // Non-0
    uint64_t    reply_to = {}; // Non-0 if this is a reply
    uint64_t    likes    = {};
    eosio::name user     = {};
    std::string content  = {};

    uint64_t primary_key() const { return id; }
    uint64_t get_reply_to() const { return reply_to; }
};

using message_table = eosio::multi_index<
    "message"_n, message, eosio::indexed_by<"by.reply.to"_n, eosio::const_mem_fun<message, uint64_t, &message::get_reply_to>>>;

// Likes table
struct [[eosio::table("likes"), eosio::contract("talk")]] likes {
    eosio::name user = {};
    std::set<uint64_t> messages;

    auto primary_key() const { return user.value; }
};

using likes_table = eosio::multi_index<"likes"_n, likes>;

// The contract
class talk : eosio::contract {
  public:
    // Use contract's constructor
    using contract::contract;

    // Post a message
    [[eosio::action]] void post(uint64_t id, uint64_t reply_to, eosio::name user, const std::string& content) {
        message_table table{get_self(), 0};

        // Check user
        require_auth(user);

        // Check reply_to exists
        if (reply_to)
            table.get(reply_to);

        // Create an ID if user didn't specify one
        eosio::check(id < 1'000'000'000ull, "user-specified id is too big");
        if (!id)
            id = std::max(table.available_primary_key(), 1'000'000'000ull);

        // Record the message
        table.emplace(get_self(), [&](auto& message) {
            message.id       = id;
            message.reply_to = reply_to;
            message.likes    = 0;
            message.user     = user;
            message.content  = content;
        });
    }

    [[eosio::action]] void like(uint64_t id, eosio::name user) {
        likes_table likes{get_self(), 0};
        message_table messages{get_self(), 0};
        
        require_auth(user);

        auto msg_itr = messages.find(id);

        // Check if the post exists at all.
        eosio::check(
            msg_itr != messages.end(),
            "No post exists with id: " + std::to_string(id)
        );
   
        auto like_itr = likes.find(user.value);

        if (like_itr != likes.end())
        {
            // This user has liked something before.
            // Check if they have already liked the post with the id.

            eosio::check(
                like_itr->messages.find(id) == like_itr->messages.end(),
                user.to_string() + " has already liked that post."
            );

            likes.modify(
                like_itr,
                get_self(),
                [&] (auto& row) {
                    row.messages.insert(id);
                }
            );
        }
        else
        {
            // First time we've encountered this user
            // add a entry for them in the likes table.

            likes.emplace(
                get_self(), 
                [&](auto& row) {
                    row.user = user;
                    row.messages.insert(id);
                }
            );
        }

        messages.modify(
            msg_itr,
            get_self(),
            [&] (auto& row) {
                row.likes += 1;
            }
        );
    }
};

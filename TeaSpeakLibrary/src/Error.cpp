//
// Created by wolverindev on 17.10.17.
//

#include "./query/command3.h"
#include "Error.h"

using namespace ts;

#define str(x) #x

#define define_error_description(type, description) \
{ error::type, str(type), description }

const std::vector<ErrorType> ts::availableErrors = {
        {0x0000, "ok"                                   , "ok"                                                         },
        {0x0001, "undefined"                            , "undefined error"                                            },
        {0x0002, "not_implemented"                      , "not implemented"                                            },
        {0x0005, "lib_time_limit_reached"               , "library time limit reached"                                 },

        {0x0100, "command_not_found"                    , "command not found"                                          },
        {0x0101, "unable_to_bind_network_port"          , "unable to bind network port"                                },
        {0x0102, "no_network_port_available"            , "no network port available"                                  },

        {0x0200, "client_invalid_id"                    , "invalid clientID"                                           },
        {0x0201, "client_nickname_inuse"                , "nickname is already in use"                                 },
        {0x0202, ""                                     , "invalid error code"                                         },
        {0x0203, "client_protocol_limit_reached"        , "max clients protocol limit reached"                         },
        {0x0204, "client_invalid_type"                  , "invalid client type"                                        },
        {0x0205, "client_already_subscribed"            , "already subscribed"                                         },
        {0x0206, "client_not_logged_in"                 , "not logged in"                                              },
        {0x0207, "client_could_not_validate_identity"   , "could not validate client identity"                         },
        {0x0208, "client_invalid_password"              , "invalid loginname or password"                              },
        {0x0209, "client_too_many_clones_connected"     , "too many clones already connected"                          },
        {0x020A, "client_version_outdated"              , "client version outdated, please update"                     },
        {0x020B, "client_is_online"                     , "client is online"                                           },
        {0x020C, "client_is_flooding"                   , "client is flooding"                                         },
        {0x020D, "client_hacked"                        , "client is modified"                                         },
        {0x020E, "client_cannot_verify_now"             , "can not verify client at this moment"                       },
        {0x020F, "client_login_not_permitted"           , "client is not permitted to log in"                          },
        {0x0210, "client_not_subscribed"                , "client is not subscribed to the channel"                    },
        {0x0211, "client_unknown"                       , "client is not known"                                        },
        {0x0212, "client_join_rate_limit_reached"       , "client has reached his join attempt limit"                  },
        {0x0213, "client_is_already_member_of_group"    , "client is already a member of the group"                    },
        {0x0214, "client_is_not_member_of_group"        , "client is not a member of the group"                        },
        {0x0215, "client_type_is_not_allowed"           , "client type is not allowed to join the server"              },

        {0x0300, "channel_invalid_id"                   , "invalid channelID"                                          },
        {0x0301, "channel_protocol_limit_reached"       , "max channels protocol limit reached"                        },
        {0x0302, "channel_already_in"                   , "already member of channel"                                  },
        {0x0303, "channel_name_inuse"                   , "channel name is already in use"                             },
        {0x0304, "channel_not_empty"                    , "channel not empty"                                          },
        {0x0305, "channel_can_not_delete_default"       , "can not delete default channel"                             },
        {0x0306, "channel_default_require_permanent"    , "default channel requires permanent"                         },
        {0x0307, "channel_invalid_flags"                , "invalid channel flags"                                      },
        {0x0308, "channel_parent_not_permanent"         , "permanent channel can not be child of non permanent channel"},
        {0x0309, "channel_maxclients_reached"           , "channel maxclient reached"                                  },
        {0x030A, "channel_maxfamily_reached"            , "channel maxfamily reached"                                  },
        {0x030B, "channel_invalid_order"                , "invalid channel order"                                      },
        {0x030C, "channel_no_filetransfer_supported"    , "channel does not support filetransfers"                     },
        {0x030D, "channel_invalid_password"             , "invalid channel password"                                   },
        {0x030E, "channel_is_private_channel"           , "channel is private channel"                                 },
        {0x030F, "channel_invalid_security_hash"        , "invalid security hash supplied by client"                   },
        {0x0310, "channel_is_deleted"                   , "target channel is deleted"                                  },
        {0x0311, "channel_name_invalid"                 , "channel name is invalid"                                    },
        {0x0312, "channel_limit_reached"                , "the virtualserver channel limit has been reached"           },
        define_error_description(channel_family_not_visible, "the channel family isn't visible by default"),
        define_error_description(channel_default_require_visible, "the channel family contains the default channel"),

        {0x0400, "server_invalid_id"                    , "invalid serverID"                                           },
        {0x0401, "server_running"                       , "server is running"                                          },
        {0x0402, "server_is_shutting_down"              , "server is shutting down"                                    },
        {0x0403, "server_maxclients_reached"            , "server maxclient reached"                                   },
        {0x0404, "server_invalid_password"              , "invalid server password"                                    },
        {0x0405, "server_deployment_active"             , "deployment active"                                          },
        {0x0406, "server_unable_to_stop_own_server"     , "unable to stop own server in your connection class"         },
        {0x0407, "server_is_virtual"                    , "server is virtual"                                          },
        {0x0408, "server_wrong_machineid"               , "server wrong machineID"                                     },
        {0x0409, "server_is_not_running"                , "server is not running"                                      },
        {0x040A, "server_is_booting"                    , "server is booting up"                                       },
        {0x040B, "server_status_invalid"                , "server got an invalid status for this operation"            },
        {0x040C, "server_modal_quit"                    , "server modal quit"                                          },
        {0x040D, "server_version_outdated"              , "server version is too old for command"                      },
        {0x040D, "server_already_joined"                , "query client already joined to the server"                  },
        {0x040E, "server_is_not_shutting_down"          , "server isn't shutting down"                                 },
        {0x040F, "server_max_vs_reached"                , "You reached the maximal virtual server limit"               },
        {0x0410, "server_unbound"                       , "you are not bound to any server"                            },
        {0x0411, "server_join_rate_limit_reached"       , "the server reached his join attempt limit"                  },


        {0x0500, "sql"                                  , "sql error"                                                  },
        {0x0501, "database_empty_result"                , "sql empty result set"                                       },
        {0x0502, "database_duplicate_entry"             , "sql duplicate entry"                                        },
        {0x0503, "database_no_modifications"            , "sql no modifications"                                       },
        {0x0504, "database_constraint"                  , "sql invalid constraint"                                     },
        {0x0505, "database_reinvoke"                    , "sql reinvoke command"                                       },

        {0x0600, "parameter_quote"                      , "invalid quote"                                              },
        {0x0601, "parameter_invalid_count"              , "invalid parameter count"                                    },
        {0x0602, "parameter_invalid"                    , "invalid parameter"                                          },
        {0x0603, "parameter_not_found"                  , "parameter not found"                                        },
        {0x0604, "parameter_convert"                    , "convert error"                                              },
        {0x0605, "parameter_invalid_size"               , "invalid parameter size"                                     },
        {0x0606, "parameter_missing"                    , "missing required parameter"                                 },
        {0x0607, "parameter_checksum"                   , "invalid checksum"                                           },
        define_error_description(parameter_constraint_violation, "parameter does not fits its constraint"),
        /* example: all name= parameter in a bulk must start with /icon_ */

        {0x0700, "vs_critical"                          , "virtual server got a critical error"                        },
        {0x0701, "connection_lost"                      , "Connection lost"                                            },
        {0x0702, "not_connected"                        , "not connected"                                              },
        {0x0703, "no_cached_connection_info"            , "no cached connection info"                                  },
        {0x0704, "currently_not_possible"               , "currently not possible"                                     },
        {0x0705, "failed_connection_initialisation"     , "failed connection initialization"                           },
        {0x0706, "could_not_resolve_hostname"           , "could not resolve hostname"                                 },
        {0x0707, "invalid_server_connection_handler_id" , "invalid server connection handler ID"                       },
        {0x0708, "could_not_initialise_input_client"    , "could not initialize Input client"                          },
        {0x0709, "clientlibrary_not_initialised"        , "client library not initialized"                             },
        {0x070A, "serverlibrary_not_initialised"        , "server library not initialized"                             },
        {0x070B, "whisper_too_many_targets"             , "too many whisper targets"                                   },
        {0x070C, "whisper_no_targets"                   , "no whisper targets found"                                   },
        {0x0800, "file_invalid_name"                    , "invalid file name"                                          },
        {0x0801, "file_invalid_permissions"             , "invalid file permissions"                                   },
        {0x0802, "file_already_exists"                  , "file already exists"                                        },
        {0x0803, "file_not_found"                       , "file not found"                                             },
        {0x0804, "file_io_error"                        , "file input/output error"                                    },
        {0x0805, "file_invalid_transfer_id"             , "invalid file transfer ID"                                   },
        {0x0806, "file_invalid_path"                    , "invalid file path"                                          },
        {0x0807, "file_no_files_available"              , "no files available"                                         },
        {0x0808, "file_overwrite_excludes_resume"       , "overwrite excludes resume"                                  },
        {0x0809, "file_invalid_size"                    , "invalid file size"                                          },
        {0x080A, "file_already_in_use"                  , "file already in use"                                        },
        {0x080B, "file_could_not_open_connection"       , "could not open file transfer connection"                    },
        {0x080C, "file_no_space_left_on_device"         , "no space left on device (disk full?)"                       },
        {0x080D, "file_exceeds_file_system_maximum_size", "file exceeds file system's maximum file size"               },
        {0x080E, "file_transfer_connection_timeout"     , "file transfer connection timeout"                           },
        {0x080F, "file_connection_lost"                 , "lost file transfer connection"                              },
        {0x0810, "file_exceeds_supplied_size"           , "file exceeds supplied file size"                            },
        {0x0811, "file_transfer_complete"               , "file transfer complete"                                     },
        {0x0812, "file_transfer_canceled"               , "file transfer canceled"                                     },
        {0x0813, "file_transfer_interrupted"            , "file transfer interrupted"                                  },
        {0x0814, "file_transfer_server_quota_exceeded"  , "file transfer server quota exceeded"                        },
        {0x0815, "file_transfer_client_quota_exceeded"  , "file transfer client quota exceeded"                        },
        {0x0816, "file_transfer_reset"                  , "file transfer reset"                                        },
        {0x0817, "file_transfer_limit_reached"          , "file transfer limit reached"                                },
        define_error_description(file_api_timeout, "the file API call has been timed out"),
        define_error_description(file_virtual_server_not_registered, "the file server does not know our virtual server"),

        define_error_description(file_server_transfer_limit_reached, "the file server reached his max concurrent transfers limit"),
        define_error_description(file_client_transfer_limit_reached, "you reached your max concurrent transfers limit"),

        {0x0A08, "server_insufficeient_permissions"     , "insufficient client permissions"                            },

        {0x0B01, "accounting_slot_limit_reached"        , "max slot limit reached"                                     },

        {0x0D01, "server_connect_banned"                , "connection failed, you are banned"                          },
        {0x0D03, "ban_flooding"                         , "flood ban"                                                  },

        define_error_description(token_invalid_id, "token unknown"),
        define_error_description(token_expired, "token has been expired"),
        define_error_description(token_use_limit_exceeded, "token has reached its use limit"),

        {0x1000, "web_handshake_invalid"                , "Invalid handshake"                                          },
        {0x1001, "web_handshake_unsupported"            , "Handshake intention unsupported"                            },
        {0x1002, "web_handshake_identity_unsupported"   , "Handshake identity unsupported"                             },
        {0x1003, "web_handshake_identity_proof_failed"  , "Identity proof failed"                                      },
        {0x1004, "web_handshake_identity_outdated"      , "data seems to be outdated"                                  },

        {0x1100, "music_invalid_id"                     , "invalid botID"                                              },
        {0x1101, "music_limit_reached"                  , "Server music bot limit is reached"                          },
        {0x1102, "music_client_limit_reached"           , "Client music bot limit is reached"                          },
        {0x1103, "music_invalid_player_state"           , "Invalid player state"                                       },
        {0x1104, "music_invalid_action"                 , "Invalid action"                                             },
        {0x1105, "music_no_player"                      , "Missing player instance"                                    },
        {0x1105, "music_disabled"                       , "Music bots have been disabled"                              },

        {0x2100, "playlist_invalid_id"                  , "invalid playlist id"                                        },
        {0x2101, "playlist_invalid_song_id"             , "invalid playlist song id"                                   },
        {0x2102, "playlist_already_in_use"              , "playlist is already used by another bot"                    },
        {0x2103, "playlist_is_in_use"                   , "playlist is used by another bot"                            },

        {0x2200, "conversation_invalid_id"              , "invalid conversation id"                                    },
        {0x2201, "conversation_more_data"               , "there are more messages to send"                            },
        {0x2202, "conversation_is_private"              , "the target conversation is private"                         },
        {0x2203, "conversation_not_exists"              , "the target conversation does not exists"                    },

        {0x2300, "rtc_missing_target_channel"           , "the target channel does not exists"                         },

        {0x1200, "query_not_exists"                     , "query account does not exists"                              },
        {0x1201, "query_already_exists"                 , "query account already exists"                               },
        {0x1202, "query_too_many_simultaneously_sessions", "too many simultaneously connected sessions"                },
        {0x1203, "query_maxclients_reached"             , "query server reached its limit"                             },

        {0x1300, "group_invalid_id"                     , "Invalid group id"                                           },
        {0x1301, "group_name_inuse"                     , "Group name is already in use"                               },
        define_error_description(group_not_assigned_over_this_server, "the group hasn't been assigned over this server"),
        define_error_description(group_not_empty, "the target group isn't empty"),

        {0xE000, "resource_limit_reached"               , "resource limit reached"                                     },

        define_error_description(broadcast_invalid_id, "the broadcast does not exists"),
        define_error_description(broadcast_invalid_type, "the broadcast type is invalid"),
        define_error_description(broadcast_invalid_client, "the broadcasting client does not exists"),

        {0xFFFF, "custom_error"                         , "costume"                                                    },
};
ErrorType ErrorType::Success = availableErrors[0];
ErrorType ErrorType::Costume = findError("custom_error");
ErrorType ErrorType::VSError = findError("vs_critical");
ErrorType ErrorType::DBEmpty = findError("database_empty_result");

ErrorType ts::findError(uint16_t errorId){
    for(auto elm : availableErrors)
        if(elm.errorId == errorId) return elm;
    return ErrorType{errorId, "undefined", "undefined"};
}

ErrorType ts::findError(std::string key){
    for(auto elm : availableErrors)
        if(elm.name == key) return elm;
    return ErrorType{1, key, "undefined"};
}

inline void write_command_result_error(ts::command_builder_bulk bulk, const command_result& result, const std::string_view& id_key) {
    bulk.put_unchecked(id_key, (uint32_t) result.error_code());
    bulk.put_unchecked("msg", findError(result.error_code()).message);
    if(result.is_permission_error())
        bulk.put_unchecked("failed_permid", (uint32_t) result.permission_id());
}

inline void write_command_result_detailed(ts::command_builder_bulk bulk, const command_result& result, const std::string_view& id_key) {
    auto details = result.details();
    bulk.put_unchecked(id_key, (uint32_t) details->error_id);
    bulk.put_unchecked("msg", findError(details->error_id).message);

    for(const auto& extra : details->extra_properties)
        bulk.put(extra.first, extra.second);
}

void command_result::build_error_response(ts::command_builder &builder, const std::string_view &idKey) const {
    switch(this->type()) {
        case command_result_type::error:
            write_command_result_error(builder.bulk(0), *this, idKey);
            break;
        case command_result_type::detailed:
            write_command_result_detailed(builder.bulk(0), *this, idKey);
            break;

        case command_result_type::bulked: {
            auto bulks = this->bulks();
            builder.reserve_bulks(bulks->size());
            for(size_t index{0}; index < bulks->size(); index++) {
                auto& entry = bulks->at(index);
                switch (entry.type()) {
                    case command_result_type::error:
                        write_command_result_error(builder.bulk(index), entry, idKey);
                        break;
                    case command_result_type::detailed:
                        write_command_result_detailed(builder.bulk(index), entry, idKey);
                        break;
                    case command_result_type::bulked:
                        assert(false);
                        break;
                }
            }

            if(bulks->empty()) {
                assert(false);
                builder.put_unchecked(0, idKey, (uint32_t) error::ok);
                builder.put_unchecked(0, "msg", findError(error::ok).message);
            }
            break;
        }
        default:
            assert(false);
            break;
    }
}
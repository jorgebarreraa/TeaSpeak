#pragma once

#include <vector>
#include <query/Command.h>
#include <Error.h>
#include <PermissionManager.h>
#include <src/client/ConnectedClient.h>
#include "./helpers.h"
#include "../../manager/ActionLogger.h"

namespace ts::command::bulk_parser {
    class PermissionBulkParser {
        public:
            explicit PermissionBulkParser(ts::ParameterBulk& bulk, bool parse_value) {
                if(bulk.has("permid")) {
                    auto type = bulk["permid"].as<permission::PermissionType>();
                    if ((type & PERM_ID_GRANT) != 0) {
                        type &= ~PERM_ID_GRANT;
                    }

                    this->permission_ = permission::resolvePermissionData(type);
                } else if(bulk.has("permsid")) {
                    auto permission_name = bulk["permsid"].string();
                    this->permission_ = permission::resolvePermissionData(permission_name);
                    this->grant_ = this->permission_->grantName() == permission_name;;
                } else {
                    this->error_.reset(ts::command_result{error::parameter_missing, "permid"});
                    return;
                }

                if(this->permission_->is_invalid()) {
                    this->error_.reset(ts::command_result{error::parameter_invalid});
                    return;
                }

                if(parse_value) {
                    if(!bulk.has("permvalue")) {
                        this->error_.reset(ts::command_result{error::parameter_missing, "permvalue"});
                        return;
                    }

                    this->value_ = bulk["permvalue"].as<permission::PermissionValue>();
                    this->flag_skip_ = bulk.has("permskip") && bulk["permskip"].as<bool>();
                    this->flag_negated_ = bulk.has("permnegated") && bulk["permnegated"].as<bool>();
                }
            }
            PermissionBulkParser(const PermissionBulkParser&) = delete;
            PermissionBulkParser(PermissionBulkParser&&)  noexcept = default;

            ~PermissionBulkParser() {
                this->error_.release_data();
            }

            [[nodiscard]] inline bool has_error() const {
                assert(!this->error_released_);
                return this->error_.has_error();
            }

            [[nodiscard]] inline bool has_value() const { return this->value_ != permission::undefined; }
            [[nodiscard]] inline bool is_grant_permission() const { return this->grant_; }

            [[nodiscard]] inline const auto& permission() const { return this->permission_; }
            [[nodiscard]] inline auto permission_type() const { return this->permission_->type; }

            [[nodiscard]] inline auto value() const { return this->value_; }
            [[nodiscard]] inline auto flag_skip() const { return this->flag_skip_; }
            [[nodiscard]] inline auto flag_negated() const { return this->flag_negated_; }

            [[nodiscard]] inline ts::command_result release_error() {
                assert(!std::exchange(this->error_released_, true));
                return std::move(this->error_);
            }

            inline void emplace_custom_error(ts::command_result&& result) {
                assert(!this->error_released_);
                this->error_.reset(std::forward<ts::command_result>(result));
            }

            inline void apply_to(const std::shared_ptr<permission::v2::PermissionManager>& manager, permission::v2::PermissionUpdateType mode) const {
                if(this->is_grant_permission()) {
                    this->old_value = manager->set_permission(this->permission_type(), { 0, this->value() }, permission::v2::PermissionUpdateType::do_nothing, mode);
                } else {
                    this->old_value = manager->set_permission(
                            this->permission_type(),
                            { this->value(), 0 },
                            mode,
                            permission::v2::PermissionUpdateType::do_nothing,
                            this->flag_skip(),
                            this->flag_negated()
                    );
                }
            }

            inline void log_update(
                    server::log::PermissionActionLogger& logger,
                    ServerId sid,
                    const std::shared_ptr<server::ConnectedClient>& issuer,
                    server::log::PermissionTarget target,
                    permission::v2::PermissionUpdateType mode,
                    uint64_t id1, const std::string& id1_name,
                    uint64_t id2, const std::string& id2_name
            ) const {
                if(this->is_grant_permission()) {
                    switch (mode) {
                        case permission::v2::delete_value:
                            if(!this->old_value.flags.grant_set)
                                return;

                            logger.log_permission_remove_grant(sid, issuer, target, id1, id1_name, id2, id2_name, *this->permission_, this->old_value.values.grant);
                            break;

                        case permission::v2::set_value:
                            if(this->old_value.flags.grant_set) {
                                logger.log_permission_edit_grant(sid, issuer, target, id1, id1_name, id2, id2_name, *this->permission_, this->old_value.values.grant, this->value_);
                            } else {
                                logger.log_permission_add_grant(sid, issuer, target, id1, id1_name, id2, id2_name, *this->permission_, this->value_);
                            }
                            break;
                        case permission::v2::do_nothing:
                            break;
                    }
                } else {
                    switch (mode) {
                        case permission::v2::delete_value:
                            if(!this->old_value.flags.value_set)
                                return;

                            logger.log_permission_remove_value(sid, issuer, target, id1, id1_name, id2, id2_name, *this->permission_, this->old_value.values.value, this->old_value.flags.negate, this->old_value.flags.skip);
                            break;

                        case permission::v2::set_value:
                            if(this->old_value.flags.value_set) {
                                logger.log_permission_edit_value(sid, issuer, target, id1, id1_name, id2, id2_name, *this->permission_, this->old_value.values.value, this->old_value.flags.negate, this->old_value.flags.skip, this->value_, this->flag_negated_, this->flag_skip_);
                            } else {
                                logger.log_permission_add_value(sid, issuer, target, id1, id1_name, id2, id2_name, *this->permission_, this->old_value.values.value, this->old_value.flags.negate, this->old_value.flags.skip);
                            }
                            break;
                        case permission::v2::do_nothing:
                            break;
                    }
                }
            }

            inline void apply_to_channel(const std::shared_ptr<permission::v2::PermissionManager>& manager, permission::v2::PermissionUpdateType mode, ChannelId channel_id) const {
                if(this->is_grant_permission()) {
                    this->old_value = manager->set_channel_permission(this->permission_type(), channel_id, { this->value(), true }, permission::v2::PermissionUpdateType::do_nothing, mode);
                } else {
                    this->old_value = manager->set_channel_permission(
                            this->permission_type(),
                            channel_id,
                            { this->value(), true },
                            mode,
                            permission::v2::PermissionUpdateType::do_nothing
                    );
                }
            }

            [[nodiscard]] inline bool is_group_property() const {
                return permission_is_group_property(this->permission_type());
            }

            [[nodiscard]] inline bool is_client_view_property() const {
                return permission_is_client_property(this->permission_type());
            }
        private:
            std::shared_ptr<permission::PermissionTypeEntry> permission_{nullptr};
            bool grant_{false};

            bool flag_skip_{false};
            bool flag_negated_{false};

            permission::PermissionValue value_{0};
            ts::command_result error_{error::ok};

            mutable permission::v2::PermissionContainer old_value{};
#ifndef NDEBUG
            bool error_released_{false};
#endif
    };

    class PermissionBulksParser {
        public:
            PermissionBulksParser(const PermissionBulksParser&) = delete;
            PermissionBulksParser(PermissionBulksParser&&)  noexcept = default;

            template <typename base_iterator>
            struct FilteredPermissionIterator : public base_iterator {
                public:
                    FilteredPermissionIterator() = default;
                    explicit FilteredPermissionIterator(base_iterator position, base_iterator end = {}) : base_iterator{position}, end_{end} {
                        if(*this != this->end_) {
                            const auto& entry = **this;
                            if(entry.has_error())
                                this->operator++();
                        }
                    }

                    FilteredPermissionIterator& operator++() {
                        while(true) {
                            base_iterator::operator++();
                            if(*this == this->end_) break;

                            const auto& entry = **this;
                            if(!entry.has_error()) break;
                        }
                        return *this;
                    }

                    [[nodiscard]] FilteredPermissionIterator operator++(int) const {
                        FilteredPermissionIterator copy = *this;
                        ++*this;
                        return copy;
                    }

                private:
                    base_iterator end_;
            };

            struct FilteredPermissionListIterable {
                typedef typename std::vector<PermissionBulkParser>::const_iterator const_iterator;
                public:
                    FilteredPermissionListIterable(const_iterator begin, const_iterator end) noexcept : begin_{begin}, end_{end} {}

                    FilteredPermissionIterator<const_iterator> begin() const {
                        return FilteredPermissionIterator{this->begin_, this->end_};
                    }

                    FilteredPermissionIterator<const_iterator> end() const {
                        return FilteredPermissionIterator{this->end_, this->end_};
                    }
                private:
                    const_iterator begin_;
                    const_iterator end_;
            };

            explicit PermissionBulksParser(ts::Command& command, bool parse_value) : parse_value{parse_value} {
                this->permissions_.reserve(command.bulkCount());
                for(size_t index{0}; index < command.bulkCount(); index++) {
                    this->permissions_.emplace_back(command[index], parse_value);
                }
            }

            [[nodiscard]] inline bool validate(const std::shared_ptr<server::ConnectedClient>& issuer, ChannelId channel_id) {
                auto ignore_granted_values = permission::v2::permission_granted(1, issuer->calculate_permission(permission::b_permission_modify_power_ignore, channel_id));
                if(!ignore_granted_values) {
                    auto max_value = issuer->calculate_permission(permission::i_permission_modify_power, channel_id, false);
                    if(!max_value.has_value) {
                        for(PermissionBulkParser& permission : this->permissions_) {
                            if(permission.has_error()) {
                                continue;
                            }

                            permission.emplace_custom_error(ts::command_result{permission::i_permission_modify_power});
                        }

                        return false;
                    }

                    for(size_t index{0}; index < this->permissions_.size(); index++) {
                        PermissionBulkParser& permission = this->permissions_[index];
                        if(permission.has_error()) {
                            continue;
                        }

                        if(this->parse_value && permission_require_granted_value(permission.permission_type()) && !permission::v2::permission_granted(permission.value(), max_value)) {
                            permission.emplace_custom_error(ts::command_result{permission::i_permission_modify_power});
                            continue;
                        }

                        if(!permission::v2::permission_granted(1, issuer->calculate_permission(permission.permission_type(), channel_id, true))) {
                            permission.emplace_custom_error(ts::command_result{permission::i_permission_modify_power});
                            continue;
                        }
                    }
                }
                return true;
            }

            [[nodiscard]] inline FilteredPermissionListIterable iterate_valid_permissions() const {
                return FilteredPermissionListIterable{this->permissions_.begin(), this->permissions_.end()};
            }

            [[nodiscard]] inline ts::command_result build_command_result() {
                assert(!std::exchange(this->result_created_, true));
                ts::command_result_bulk result{};

                for(auto& permission : this->permissions_) {
                    result.insert_result(std::forward<ts::command_result>(permission.release_error()));
                }

                return ts::command_result{std::move(result)};
            }
        private:
            bool parse_value;
            std::vector<PermissionBulkParser> permissions_{};
#ifndef NDEBUG
            bool result_created_{false};
#endif
    };
}
-module(tau5_discovery).

-export([init/0, is_nif_loaded/0, start/1, stop/0, list/0,
         set_notification_pid/0, clear_notification_pid/0,
         enable_nif_logging/0, disable_nif_logging/0, is_nif_logging_enabled/0]).

-define(APPLICATION, tau5).
-define(LIBNAME, "libtau5_discovery").

init() ->
    SoName = filename:join([code:priv_dir(?APPLICATION), "nifs", ?LIBNAME]),
    erlang:load_nif(SoName, 0).

is_nif_loaded() ->
    false.

start(InfoString) when is_binary(InfoString); is_list(InfoString) ->
    {error, nif_library_not_loaded}.

stop() ->
    {error, nif_library_not_loaded}.

list() ->
    {error, nif_library_not_loaded}.

set_notification_pid() ->
    {error, nif_library_not_loaded}.

clear_notification_pid() ->
    {error, nif_library_not_loaded}.

enable_nif_logging() ->
    {error, nif_library_not_loaded}.

disable_nif_logging() ->
    {error, nif_library_not_loaded}.

is_nif_logging_enabled() ->
    {error, nif_library_not_loaded}.
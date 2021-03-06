(* NTP module for Augeas                      *)
(* Author: Raphael Pinson <raphink@gmail.com> *)
(*                                            *)
(* Status: basic settings supported           *)

module Ntp =
  autoload xfm


    (* Define useful shortcuts *)

    let eol = del /[ \t]*/ "" . [ label "#comment" . store /#.*/]?
            . Util.del_str "\n"
    let sep_spc = Util.del_ws_spc
    let word = /[^,# \n\t]+/
    let num  = /[0-9]+/


    (* define comments and empty lines *)
    let comment = [ label "#comment" . del /#[ \t]*/ "#" .
                    store /([^ \t\n][^\n]*)?/ . del "\n" "\n" ]
    let empty   = [ del /[ \t]*\n/ "\n" ]


    let kv (k:regexp) (v:regexp) =
      [ key k . sep_spc. store v . eol ]

    (* Define generic record *)
    let record (kw:regexp) (value:lens) =
      [ key kw . sep_spc . store word . value . eol ]

    (* Define a command record; see confopt.html#cfg in the ntp docs *)
    let command_record =
      let opt = [ sep_spc . key /minpoll|maxpoll|ttl|version|key/ .
                      sep_spc . store word ]
        | [ sep_spc . key (/autokey|burst|iburst|noselect|preempt/ |
                           /prefer|true|dynamic/) ] in
      let cmd = /server|peer|broadcast|manycastclient/
        | /multicastclient|manycastserver/ in
        record cmd opt*

    let broadcastclient =
      [ key "broadcastclient" . [ sep_spc . key "novolley" ]? . eol ]

    (* Define a fudge record *)
    let fudge_opt_re = "refid" | "stratum"
    let fudge_opt  = [ sep_spc . key fudge_opt_re . sep_spc . store word ]
    let fudge_record = record "fudge" fudge_opt?

    (* Define simple settings, see miscopt.html in ntp docs *)
    let flags =
      let flags_re = /auth|bclient|calibrate|kernel|monitor|ntp|pps|stats/ in
      let flag = [ label "flag" . store flags_re ] in
        [ key /enable|disable/ . (sep_spc . flag)* . eol ]

    let simple_setting (k:regexp) = kv k word

    (* Still incomplete, misses logconfig, phone, setvar, tinker, tos,
       trap, ttl *)
    let simple_settings =
        kv "broadcastdelay" Rx.decimal
      | flags
      | simple_setting /driftfile|leapfile|logfile|includefile/
	  | simple_setting "statsdir"

    (* Misc commands, see miscopt.html in ntp docs *)

    (* Define restrict *)
    let restrict_record   =
      let action    = [ label "action" . sep_spc . store word ] in
      [ key "restrict" . sep_spc .
          [ label "ipv6" . Util.del_str "-6" . sep_spc ]? .
          store (word - "-6") . action* . eol ]

    (* Define statistics *)
    let statistics_flag (kw:string) = [ sep_spc . key kw ]

    let statistics_opts = statistics_flag "loopstats"
                        | statistics_flag "peerstats"
			| statistics_flag "clockstats"
			| statistics_flag "rawstats"

    let statistics_record = [ key "statistics" . statistics_opts* . eol ]


    (* Define filegen *)
    let filegen = del /filegen[ \t]+/ "filegen " . store word
    let filegen_opt (kw:string) = [ sep_spc . key kw . sep_spc . store word ]
    (* let filegen_flag (kw:string) = [ label kw . sep_spc . store word ] *)
    let filegen_select (kw:string) (select:regexp) = [ label kw . sep_spc . store select ]

    let filegen_opts = filegen_opt "file"
                     | filegen_opt "type"
		     | filegen_select "enable" /(en|dis)able/
		     | filegen_select "link" /(no)?link/

    let filegen_record = [ label "filegen" . filegen . filegen_opts* . eol ]

    (* Authentication commands, see authopt.html#cmd; incomplete *)
    let auth_command =
      [ key /controlkey|keys|keysdir|requestkey/ .
            sep_spc . store word . eol ]
     | [ key /autokey|revoke/ . [sep_spc . store word]? . eol ]
     | [ key /trustedkey/ . [ sep_spc . label "key" . store word ]+ . eol ]

    (* Define lens *)

    let lns = ( comment | empty | command_record | fudge_record
              | restrict_record | simple_settings | statistics_record
              | filegen_record | broadcastclient
              | auth_command )*

    let filter = (incl "/etc/ntp.conf")
        . Util.stdexcl

    let xfm = transform lns filter

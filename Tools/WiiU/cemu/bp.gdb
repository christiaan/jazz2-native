set pagination off
target remote localhost:1337
break opendir
commands
silent
printf "opendir\n"
bt 4
continue
end
break closedir
commands
silent
printf "closedir\n"
bt 4
continue
end
break realpath
commands
silent
printf "realpath\n"
continue
end
break __wut_fsa_dirclose
commands
silent
printf "fsa_dirclose\n"
continue
end
break __wut_fsa_diropen
commands
silent
printf "fsa_diropen\n"
continue
end
continue
info threads
bt 30

#!/bin/bash
# В Debian для работы с sudo необходимо добавить себя в группу sudo:
# apt-get install sudo
# groupadd sudo
# usermod -a -G %username% sudo
# и выполнить перезагрузку

# константы
MAX_FILE_NAME=63
SELECT_ALL="Выбрать все файлы"
MIN_SAMPLE_LEN=15
PNAME="audio-book-encoder"

# имена файлов
pref="${HOME}/.config/$PNAME/"
conf="${pref}$(basename ${0}).conf"
#cur="${pref}$(basename ${0}).current"
pl="${pref}$(basename ${0}).plist"
initials="${pref}$(basename ${0}).init"
#lock="/tmp/$(basename "${0}").lock"
sound_path="/usr/share/sounds/$PNAME"

# имена временных файлов
tmp0=$(/bin/mktemp)
tmp1=${tmp0}1
tmp2=${tmp0}2
tmp3=${tmp0}3

# глобальные переменные
script_path=
work_path=
volume=
rate=
pass=
distr=
power_off=
print_buffer=

# процедуры
play_beep(){
	numb=
	if [ ${1} ]; then numb=${1}; else numb=2; fi
	if [ ! ${numb} = "0" ];then
		play -q "${sound_path}/snd${numb}.mp3" &
	fi
}

message(){
	# Вывод сообщений
	echo "${2}"
	snd=0
	if [ "${1}" = "0" ];then caption="Ошибка!!!"; snd=13; else caption="${1}"; if [ ${4} ]; then snd=${4};fi fi
	if [ ${snd} -gt 0 ];then play_beep ${snd};fi
	if [ ! ${3} ]; then tt=5; else tt=${3}; fi
	if [ ${tt} -gt 4 ]; then nstime=5000; else nstime=$((tt*1000)); fi
	if [ ${tt} -gt 0 ];then notify-send "${caption}" "${2}" -t ${nstime};fi
	if [[ ${tt} -gt 0 ]];then
		((tt*=4)); cnt=$tt; while [ $cnt -gt 0 ];do
			echo "# \r\n${2} <<<$(($cnt/4))>>>"
			((cnt--)); echo $(($cnt*100/${tt})); sleep .25
		done | zenity --title="${caption}" --progress --auto-close --width 400
		if [ ! $? = 0 ];then return 64; fi
	else zenity --info --title="${caption}" --text="${2}" --timeout=5; fi
}

get_pass(){
	if [ ! ${pass} ];then
		if [ ! $(which gksu) ]; then echo "Не установлен gksu! Аварийный выход!"; quit_script
		else pass=$(gksu -m "Введи пароль пользователя:" -p)
	fi fi
}

install_pkg(){
	while [[ "${1}_" != "_" ]]; do
		if [ ! $(which ${1}) ];then
			get_pass
			echo "Устанавливаю пакет: \"${1}\""
			if [ ${distr} -eq 1 ];then echo "${pass}"|sudo -S apt-get install -y "${1}"
			else echo "${pass}"|sudo -S pacman --noconfirm -S "${1}"; fi
		fi
		shift
	done
}

update_conf(){
	# сохраняю параметры по-умолчанию
	if [ ! "${work_path}_" = "_" ];	then echo "Path: ${work_path}" > "${conf}"; fi
	if [ ! "${rate}_" = "_" ];		then echo "Rate: ${rate}" >> "${conf}"; fi
	if [ ! "${volume}_" = "_" ];	then echo "Volume: ${volume}" >> "${conf}"; fi
}

set_def_conf(){
	cd
	work_path=${PWD}
	rate=$1
	volume=$2
	update_conf
}

get_conf(){
	# читаю сохраненные параметры
	if [ -d "${pref}" ];then
		if [ -f "$conf" ];then
			work_path=$(grep 'Path:' ${conf} | cut -f2 -d: | sed 's/^[ \t]*//'); cd "${work_path}"
			rate=$(grep 'Rate' "${conf}" | awk '{print $2}')
			volume=$(grep 'Volume' "${conf}" | awk '{print $2}')
		else set_def_conf $1 $2; fi
	else mkdir "${pref}"; set_def_conf $1 $2; fi
}

install_utils(){
# установка пакетов для Debian-Ubuntu-Arch
if [ -f "${initials}" ]; then return; fi
distr=0
dist_name=$(grep -ihs "buntu\|Debian\|Arch" /etc/{issue,*release,*version}|head -n 1)
if [[ "$(echo "${dist_name}" | grep "Ubuntu")"_ != "_" ]] || [[ "$(echo "${dist_name}" | grep "Debian")"_ != "_" ]];then distr=1
else if [[ "$(echo "${dist_name}" | grep "Arch")"_ != "_" ]]; then distr=2; fi; fi
if [ ${distr} -eq 0 ]; then play_beep
	echo "Неподходящий дистрибутив.\r\nТолько для Debian, Ubuntu и Arch linux!"
	quit_script; fi

install_pkg zenity mpg123 lame
if [ ! $(which notify-send) ];then if [ ${distr} -eq 1 ];then install_pkg libnotify-bin;else install_pkg libnotify;fi;fi
	
# установка кодеков
lib_name="vo-amrwbenc"
if [ ${distr} -eq 1 ];then a=$(dpkg -s "lib${lib_name}0"|grep installed)	
	if [ "$a"_ = "_" ];then install_pkg "lib${lib_name}0" libopencore-amrwb0; fi
else if [ ${distr} -eq 2 ];then res=$(yaourt -Ss ${lib_name} | grep "installed")
	if [ "${res}_" = "_" ]; then yaourt -S ${lib_name}; fi fi fi

# установка и компиляция утилиты ffmpeg
res=""; if [ $(which ffmpeg) ]; then ffprobe -codecs > "${tmp3}" 2>&1; res=$(cat "${tmp3}" | grep ${lib_name}); rm ${tmp3}; fi
if [ ! $(which ffmpeg) ] || [ "${res}_" = "_" ];then
	mes=
	if [ "${res}_" = "_" ]; then mes="Установленный в системе пакет ffmpeg собран без поддержки библиотеки ${lib_name}!"
	else mes="Отсутствует пакет ffmpeg!"; fi
	zenity --title="$CAPT" --question --height=300 --width=400 --text="Внимание\!\r\n${mes}\r\nДля работы скрипта необходимо скачать с git исходники и скомпилировать утилиту ffmpeg с поддержкой библиотеки AMR-WB encoder.\r\nЭто может занять продолжительное время, для слежения за процессом лучше всего запустить этот сркрипт в терминале\!\r\nПриступить?"
	if [ $? -eq 0 ];then
		if [ ${distr} -eq 1 ]; then install_pkg libsdl-dev git yasm pkg-config build-essential libvo-amrwbenc-dev libopencore-amrwb-dev libopencore-amrnb-dev
		else install_pkg git yasm ; fi
		cd /tmp; git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg; cd ffmpeg
		./configure --enable-ffplay --enable-version3 --enable-libopencore-amrwb --enable-libvo-amrwbenc
		make
		if [ $(which ffmpeg) ]; then
			echo "Удаляю установленный ffmpeg...";
			if [ ${distr} -eq 1 ];then echo "${pass}"|sudo -S apt-get remove ffmpeg
			else echo "${pass}"|sudo -S pacman -R ffmpeg; fi fi
		echo "Устанавливаю свежеиспеченную утилиту..."
		echo "${pass}"|sudo -S make install; cd
		echo "Разрешаю скрипту отключать компьютер"
		echo "${pass}"|sudo -S chmod +s /sbin/poweroff
	else message "${CAPT}" "Без утилиты ffmpeg продолжение невозможно!" 10; quit_script; fi fi
	echo "codecs & pakages for this script is setup and inicialised" > "${initials}"
}

print_digit(){
	# вывод чисел (пример использования массивов в bash)
	sotny=( _ сто двести триста четыреста пятьсот шестьсот семьсот восемьсот девятьсот )
	desyatky=( _ _ двадцать тридцать сорок пятьдесят сетьдесят семьдесят восемьдесят девяносто )
	decymal=( десять один две три четыр пят шест сем восем девят )
	edinicy=( ноль один два три четыре пять шесть семь восемь девять десять )
	edinicy1=( ноль одна две три четыре пять шесть семь восемь девять десять )
	dec_tail="надцать"

	digit=${2}
	if [ ${digit} -lt 10 ]; then digit="00${digit}"; else if [ ${digit} -lt 100 ]; then digit="0${digit}"; fi fi
	sot=${digit:0:1}
	des=${digit:1:1}
	edn=${digit:2:1}

	result=
	if [ ${sot} -gt 0 ]; then result="${sotny[sot]}"; fi
	if [ ${des} -eq 1 ];then result="${result} ${decymal[edn]}"
		if [ ! ${edn} -eq 0 ]; then result=${result}${dec_tail}; fi
	else
		if [ ${des} -gt 1 ]; then result="${result} ${desyatky[des]}"; fi
		if [ ${digit} -eq 0 ];then result=${edinicy[$digit]}
		else if [ ${edn} -gt 0 ]; then if [ "${3}" = "секунд" ]; then
			 result="${result} ${edinicy1[edn]}"
		else result="${result} ${edinicy[edn]}"
	fi fi fi fi

	local tail="${4}"
	s=$((${2}%10))
	if [[ ${s} -eq 1 ]];then tail="${5}"
	else if [[ ${s} -gt 1 && ${s} -lt 5 ]];then	tail="${6}";fi fi
	s=$((${2}/10))
	if [[ ${s} -eq 1 ]];then tail="${4}";fi
	print_buffer="${1}${result} ${3}${tail}"
}

rate_setup(){
# запрашиваем битрейт
IFS=$"|"
rates="FALSE|6600|FALSE|8850|TRUE|12650|FALSE|14250|FALSE|15850|FALSE|18250|FALSE|19850|FALSE|23050|FALSE|23850"
zenity --question --title="Выбор битрейта" \
	--text="Выбран битрейт для формата AWB(Adaptive Multi-Rate WideBand):\
	\r\n$rate kbit/sec.\r\nИзменить?"
if [ $? -eq 0 ];then
	rt=$(zenity --title="Выбор битрейта" --list --radiolist \
		--text="Выбери битрейт" --column="Выбор" \
		--column="Битрейт bit/sec" $rates)
	if [ $? -eq 0 ];then rate=${rt};fi
fi
unset IFS
}

power_dialog(){
	power_off="Off"
	zenity --question --title="${CAPT}" --text="Отключить компьютер по завершении задания?" --cancel-label="Отключить" --ok-label="Ненадо"
	if [ ! $? -eq 0 ];then power_off="On"; notify-send "$CAPT" "По завершении задания компьютер будет отключен!" -t 3000; fi
}

poweroff(){
	echo "${print_buffer}"
	if [ "${power_off}" = "On" ];then
	# диалог завершения работы компьютера
		cnt=179; while [ $cnt -gt 0 ];do
			cnt1=$((cnt/3))
			print_digit "# До отключения осталось" ${cnt1} "секунд" "" "a" "ы"
			echo "${print_buffer}"
			echo $(($cnt1*100/60))
			if [ $((cnt1/10)) -eq 0 ];then play_beep 1;else if [ $((cnt1%10)) -eq 5 ];then play_beep 6;fi fi
			((cnt--)); sleep .33
		done | zenity --title="Отключение компьютера..." --progress --auto-close --width 400;
		if [ ! $? -eq 0 ];then return
		else
#			rm "${lock}"
			play_beep 3; /sbin/poweroff
	fi fi
}

quit_script(){
#	rm "${lock}"
	exit 0
}

# сохраняю путь к папке со скриптом
script_path="`dirname $(readlink -f $0)`"
power_off="Off"
#if [ -f "${lock}" ]; then echo "Скрипт уже запущен!"; play_beep 1; exit 0; else touch "${lock}"; fi

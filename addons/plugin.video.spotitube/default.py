#!/usr/bin/python
# -*- coding: utf-8 -*-
import os
import re
import sys
reload(sys)
sys.setdefaultencoding('utf-8')
try: import simplejson as json
except ImportError: import json
import random
import socket
import urllib
import urllib2
import urlparse
import datetime
import xbmc
import xbmcplugin
import xbmcgui
import xbmcaddon
import xbmcvfs
import requests
import time
from operator import itemgetter
from StringIO import StringIO
import gzip


pluginhandle = int(sys.argv[1])
socket.setdefaulttimeout(30)
addonID = 'plugin.video.spotitube'
addon = xbmcaddon.Addon(id=addonID)
addonPath = xbmc.translatePath(addon.getAddonInfo('path')).decode('utf-8')
dataPath = os.path.join(xbmc.translatePath('special://profile/addon_data/'+addonID), '')
translation = addon.getLocalizedString
region = xbmc.getLanguage(xbmc.ISO_639_1, region=True).split("-")[1]
icon = os.path.join(addonPath, 'icon.png').decode('utf-8')
fanart = os.path.join(addonPath, 'fanart.jpg').decode('utf-8')
pic = xbmc.translatePath(os.path.join(addonPath, 'resources/images/'))
blacklist = addon.getSetting("blacklist").split(',')
infoEnabled = addon.getSetting("showInfo") == "true"
infoType = addon.getSetting("infoType")
infoDelay = int(addon.getSetting("infoDelay"))
infoDuration = int(addon.getSetting("infoDuration"))
useThumbAsFanart = addon.getSetting("useThumbAsFanart") == 'true'
cachePath = os.path.join(xbmc.translatePath(addon.getSetting("cacheDir")))
cacheDays = int(addon.getSetting("cacheLong"))
itunesShowSubGenres = addon.getSetting("itunesShowSubGenres") == 'true'
itunesForceCountry = addon.getSetting("itunesForceCountry") == 'true'
itunesCountry = addon.getSetting("itunesCountry")
spotifyForceCountry = addon.getSetting("spotifyForceCountry") == 'true'
spotifyCountry = addon.getSetting("spotifyCountry")
forceView = addon.getSetting("forceView") == 'true'
viewIDGenres = str(addon.getSetting("viewIDGenres"))
viewIDPlaylists = str(addon.getSetting("viewIDPlaylists"))
viewIDVideos = str(addon.getSetting("viewIDVideos"))
urlMainBB = "http://www.billboard.com"
urlMainBP = "https://www.beatport.com"
urlMainHypem = "http://hypem.com"
urlMainOC = "http://www.officialcharts.com"
api_key = "AIzaSyCIM4EzNqi1in22f4Z3Ru3iYvLaY8tc3bo"
	
if itunesForceCountry and itunesCountry:
	iTunesRegion = itunesCountry
else:
	iTunesRegion = region
	
if spotifyForceCountry and spotifyCountry:
	spotifyRegion = spotifyCountry
else:
	spotifyRegion = region
	
if not os.path.isdir(dataPath):
	os.makedirs(dataPath)
	
if cachePath == "":
	addon.setSetting(id='cacheDir', value='special://profile/addon_data/'+addonID+'/cache')
elif cachePath != "" and not os.path.isdir(cachePath) and not cachePath.startswith(('smb://', 'nfs://', 'upnp://', 'ftp://')):
	os.mkdir(cachePath)
elif cachePath != "" and not os.path.isdir(cachePath) and cachePath.startswith(('smb://', 'nfs://', 'upnp://', 'ftp://')):
	addon.setSetting(id='cacheDir', value='special://profile/addon_data/'+addonID+'/cache') and os.mkdir(cachePath)
elif cachePath != "" and os.path.isdir(cachePath):
		xDays = cacheDays # Days after which Files would be deleted
		now = time.time() # Date and time now
		for root, dirs, files in os.walk(cachePath):
			for name in files:
				filename = os.path.join(root, name).decode('utf-8')
				if os.path.exists(filename):
					if os.path.getmtime(filename) < now - (60*60*24*xDays): # Check if CACHE-File exists and remove CACHE-File after defined xDays
						os.remove(filename)
	
	
def index():
	addDir(translation(40101), "", "bpMain", pic+'beatport.png')
	addDir(translation(40102), "", "billboardMain", pic+'billboard.png')
	addDir(translation(40103), "", "hypemMain", pic+'hypem.png')
	addDir(translation(40104), "", "itunesMain", pic+'itunes.png')
	addDir(translation(40105), "", "ocMain", pic+'official.png')
	addDir(translation(40106), "", "spotifyMain", pic+'spotify.png')
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def billboardMain():
	addAutoPlayDir(translation(40107), urlMainBB+"/rss/charts/hot-100", "listBillboardCharts", "", "", "browse")
	addAutoPlayDir(translation(40108), "Top 140 in Emerging", "listBillboardChartsNew", "", "", "browse")
	addAutoPlayDir(translation(40109), "Top 140 in Overall", "listBillboardChartsNew", "", "", "browse")
	addDir(translation(40110), "", "listBillboardArchiveMain", "", "", "browse")
	addDir(translation(40111), "genre", "listBillboardChartsTypes", "", "", "browse")
	addDir(translation(40112), "country", "listBillboardChartsTypes", "", "", "browse")
	addDir(translation(40113), "other", "listBillboardChartsTypes", "", "", "browse")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def listBillboardArchiveMain():
	for i in range(datetime.date.today().year,1957,-1):
		addAutoPlayDir(str(i), urlMainBB+"/archive/charts/"+str(i), "listBillboardArchive", "", "", "browse")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def listBillboardArchive(url):
	content = cache(url, 30)
	match = re.compile('<span class="field-content">\s*<a href="(.*?)">(.*?)</a>', re.DOTALL).findall(content)
	for url, title in match:
		if not "billboard 200" in title.lower() and not "album" in title.lower():
			addAutoPlayDir(cleanTitle(title), urlMainBB+url, "listBillboardArchiveVideos", "", "", "browse")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def listBillboardArchiveVideos(type, url, limit):
	if type == "play":
		musicVideos = []
		playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
		playlist.clear()
	content = cache(url, 30)
	match = re.compile('views-field-field-chart-item-song.+?>(.*?)<.+?href="/artist/.+?">(.*?)</a>', re.DOTALL).findall(content)
	pos = 1
	for title, artist in match:
		title = title.strip()
		if title.lower() != "song":
			title = cleanTitle(artist+" - "+title)
			if title.isupper():
				title = title.title()
			filtered = False
			for entry2 in blacklist:
				if entry2.strip().lower() and entry2.strip().lower() in title.lower():
					filtered = True
			if filtered:
				continue
			if type == "browse":
				addLink(title, title.replace(" - ", " "), "playYTByTitle", "")
			else:
				url = "plugin://"+addonID+"/?url="+urllib.quote_plus(title.replace(" - ", " "))+"&mode=playYTByTitle"
				musicVideos.append([title, url, ""])
				if limit and int(limit) == pos:
					break
				pos += 1
	if type == "browse":
		xbmcplugin.endOfDirectory(pluginhandle)
	else:
		random.shuffle(musicVideos)
		for title, url, thumb in musicVideos:
			listitem = xbmcgui.ListItem(title, thumbnailImage=thumb)
			playlist.add(url, listitem)
		xbmc.Player().play(playlist)
	
	
def listBillboardChartsTypes(type):
	if type == "genre":
		addAutoPlayDir(translation(40114), urlMainBB+"/rss/charts/pop-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40115), urlMainBB+"/rss/charts/rock-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40116), urlMainBB+"/rss/charts/alternative-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40117), urlMainBB+"/rss/charts/r-b-hip-hop-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40118), urlMainBB+"/rss/charts/r-and-b-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40119), urlMainBB+"/rss/charts/rap-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40120), urlMainBB+"/rss/charts/country-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40121), urlMainBB+"/rss/charts/latin-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40122), urlMainBB+"/rss/charts/jazz-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40123), urlMainBB+"/rss/charts/dance-club-play-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40124), urlMainBB+"/rss/charts/dance-electronic-songs", "listBillboardCharts", "", "", "browse")
	elif type == "country":
		addAutoPlayDir(translation(40125), urlMainBB+"/rss/charts/canadian-hot-100", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40126), urlMainBB+"/rss/charts/k-pop-hot-100", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40127), urlMainBB+"/rss/charts/japan-hot-100", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40128), urlMainBB+"/rss/charts/germany-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40129), urlMainBB+"/rss/charts/france-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40130), urlMainBB+"/rss/charts/united-kingdom-songs", "listBillboardCharts", "", "", "browse")
	elif type == "other":
		addAutoPlayDir(translation(40131), urlMainBB+"/rss/charts/radio-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40132), urlMainBB+"/rss/charts/digital-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40133), urlMainBB+"/rss/charts/streaming-songs", "listBillboardCharts", "", "", "browse")
		addAutoPlayDir(translation(40134), urlMainBB+"/rss/charts/on-demand-songs", "listBillboardCharts", "", "", "browse")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def listBillboardCharts(type, url, limit):
	if type == "play":
		musicVideos = []
		playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
		playlist.clear()
	content = cache(url, 1)
	match = re.compile('<item>\s*<title>(.*?)</title>.+?<artist>(.*?)</artist>.+?<chart_item_title>(.*?)</chart_item_title>', re.DOTALL).findall(content)
	pos = 1
	for position, artist, title in match:
		position = position[:position.find(":")]
		title = cleanTitle(artist + " - " +title)
		if type == "browse":
			name = '[B][COLOR chartreuse]'+position+' •  [/COLOR]'+title+'[/B]'
			addLink(name, title.replace(" - ", " "), "playYTByTitle", "")
		else:
			url = "plugin://"+addonID+"/?url="+urllib.quote_plus(title.replace(" - ", " "))+"&mode=playYTByTitle"
			musicVideos.append([title, url, ""])
			if limit and int(limit) == pos:
				break
			pos += 1
	if type == "browse":
		xbmcplugin.endOfDirectory(pluginhandle)
	else:
		random.shuffle(musicVideos)
		for title, url, thumb in musicVideos:
			listitem = xbmcgui.ListItem(title, thumbnailImage=thumb)
			playlist.add(url, listitem)
		xbmc.Player().play(playlist)
	
	
def listBillboardChartsNew(type, url, limit):
	if type == "play":
		musicVideos = []
		playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
		playlist.clear()
	content = cache('http://realtime.billboard.com/', 1)
	content = content[content.find("<h1>"+url+"</h1>"):]
	content = content[:content.find("</table>")]
	match = re.compile('<tr>.+?<td>(.*?)</td>.+?<td><a href=".+?">(.*?)</a></td>.+?<td>(.*?)</td>.+?<td>(.*?)</td>.*?</tr>', re.DOTALL).findall(content)
	pos = 1
	for position, artist, title, signal in match:
		if "(" in title:
			title = title[:title.find("(")].strip()
		title = cleanTitle(artist+" - "+title)
		if type == "browse":
			name = '[B][COLOR chartreuse]'+position+' •  [/COLOR]'+title+'[/B]'
			addLink(name, title.replace(" - ", " "), "playYTByTitle", "")
		else:
			url = "plugin://"+addonID+"/?url="+urllib.quote_plus(title.replace(" - ", " "))+"&mode=playYTByTitle"
			musicVideos.append([title, url, ""])
			if limit and int(limit)==pos:
				break
			pos += 1
	if type == "browse":
		xbmcplugin.endOfDirectory(pluginhandle)
	else:
		random.shuffle(musicVideos)
		for title, url, thumb in musicVideos:
			listitem = xbmcgui.ListItem(title, thumbnailImage=thumb)
			playlist.add(url, listitem)
		xbmc.Player().play(playlist)
	
	
def bpMain():
	addAutoPlayDir(translation(40135), urlMainBP+"/top-100", "listBP", "", "", "browse")
	content = cache('https://pro.beatport.com', 30)
	content = content[content.find('<div class="mobile-menu-body">')+1:]
	content = content[:content.find('<!-- End Mobile Touch Menu -->')]
	spl = content.split('class="genre-drop-list-item"')
	for i in range(1,len(spl),1):
		entry = spl[i]
		genreID = re.compile('<a href="(.*?)" class=', re.DOTALL).findall(entry)[0]
		title = re.compile('"genre">(.*?)</a>', re.DOTALL).findall(entry)[0]
		name = cleanTitle(title)
		fullUrl = 'https://pro.beatport.com%s/top-100' %genreID
		addAutoPlayDir(name,fullUrl, "listBP", "", "", "browse")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def listBP(type, url, limit):
	if type == "play":
		musicVideos = []
		playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
		playlist.clear()
	content = cache(url, 1)
	spl = content.split('bucket-item ec-item track')
	pos = 1
	for i in range(1,len(spl),1):
		entry = spl[i]
		artist = re.findall('data-artist=".*?">(.*?)</a>',entry,re.S)[0]
		videoTitle = re.findall('title="(.*?)"',entry,re.S)[0]
		if "(Original Mix)" in videoTitle:
			videoTitle = videoTitle[:videoTitle.find("(Original Mix)")].strip()
		title = cleanTitle(artist+" - "+videoTitle)
		thumb = re.findall('data-src="(https:.*?.jpg)"',entry,re.S)[0].replace("/30x30/","/500x500/").replace("/60x60/","/500x500/").replace("/95x95/","/500x500/").replace("/250x250/","/500x500/")
		filtered = False
		for entry2 in blacklist:
			if entry2.strip().lower() and entry2.strip().lower() in title.lower():
				filtered = True
		if filtered:
			continue
		if type == "browse":
			addLink(title, title.replace(" - ", " "), "playYTByTitle", thumb)
		else:
			url = "plugin://"+addonID+"/?url="+urllib.quote_plus(title.replace(" - ", " "))+"&mode=playYTByTitle"
			musicVideos.append([title, url, thumb])
			if limit and int(limit)==pos:
				break
			pos += 1
	if type == "browse":
		xbmcplugin.endOfDirectory(pluginhandle)
		if forceView:
			xbmc.executebuiltin('Container.SetViewMode('+viewIDVideos+')')
	else:
		random.shuffle(musicVideos)
		for title, url, thumb in musicVideos:
			listitem = xbmcgui.ListItem(title, thumbnailImage=thumb)
			playlist.add(url, listitem)
		xbmc.Player().play(playlist)
	
	
def hypemMain():
	addAutoPlayDir(translation(40136), urlMainHypem+"/popular?ax=1&sortby=shuffle", 'listHypem', "", "", "browse")
	addAutoPlayDir(translation(40137), urlMainHypem+"/popular/lastweek?ax=1&sortby=shuffle", 'listHypem', "", "", "browse")
	addAutoPlayDir(translation(40138), "", 'listTimeMachine', "", "", "browse")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def listHypem(type, url, limit):
	musicVideos = []
	if type == "play":
		playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
		playlist.clear()
	parentUrl = url
	if url == urlMainHypem+"/popular?ax=1&sortby=shuffle":
		content = cache(url, 0)
	else:
		content = cache(url, 1)
	match = re.compile('class="rank">(.*?)</h3>', re.DOTALL).findall(content)
	spl = content.split('<span class="rank"')
	for i in range(1,len(spl),1):
		entry = spl[i]
		rank = re.compile('>(.*?)</span>', re.DOTALL).findall(entry)[0]
		match2 = re.compile('style="background.+?(http://static-ak.hypem.*?jpg)', re.DOTALL).findall(entry)
		thumb = ""
		if match2:
			#thumb = match2[0].replace('_120.jpg', '_500.jpg')
			thumb = match2[0]
		artist = re.compile('href="/artist/.+?">(.*?)</a>', re.DOTALL).findall(entry)[0]
		title = re.compile('class="base-title">(.*?)</span>', re.DOTALL).findall(entry)[0]
		remix = re.compile('class="remix-link">(.*?)</span>', re.DOTALL).findall(entry)
		if remix:
			title += ' ('+remix[0]+')'
		title = cleanTitle(artist.strip()+" - "+title.strip())
		oTitle = title
		filtered = False
		for entry2 in blacklist:
			if entry2.strip().lower() and entry2.strip().lower() in title.lower():
				filtered = True
		if filtered:
			continue
		if type == "play":
			url = "plugin://"+addonID+"/?url="+urllib.quote_plus(oTitle.replace(" - ", " "))+"&mode=playYTByTitle"
		else:
			url = oTitle
		musicVideos.append([int(rank), title, url, thumb])
	musicVideos = sorted(musicVideos, key=itemgetter(0))
	if type == "browse":
		for rank, title, url, thumb in musicVideos:
			name = '[B][COLOR chartreuse]'+str(rank)+' •  [/COLOR]'+title+'[/B]'
			addLink(name, url.replace(" - ", " "), "playYTByTitle", thumb)
		xbmcplugin.endOfDirectory(pluginhandle)
	else:
		if limit:
			musicVideos = musicVideos[:int(limit)]
		random.shuffle(musicVideos)
		for rank, title, url, thumb in musicVideos:
			listitem = xbmcgui.ListItem(title, thumbnailImage=thumb)
			playlist.add(url, listitem)
		xbmc.Player().play(playlist)
	
	
def listTimeMachine():
	for i in range(1, 210, 1):
		dt = datetime.date.today()
		while dt.weekday() != 0:
			dt -= datetime.timedelta(days=1)
		dt -= datetime.timedelta(weeks=i)
		months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
		month = months[int(dt.strftime("%m")) - 1]
		addAutoPlayDir(dt.strftime("%d. %b - %Y").replace("Mar", translation(40156)).replace("May", translation(40157)).replace("Oct", translation(40158)).replace("Dec", translation(40159)), urlMainHypem+"/popular/week:"+month+"-"+dt.strftime("%d-%Y")+"?ax=1&sortby=shuffle", 'listHypem', "", "", "browse")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def itunesMain():
	content = cache("https://itunes.apple.com/"+iTunesRegion+"/genre/music/id34", 30)
	content = content[content.find('id="genre-nav"'):]
	content = content[:content.find('</div>')]
	match = re.compile('<li><a href="https://itunes.apple.com/.+?/genre/.+?/id(.*?)"(.*?)title=".+?">(.*?)</a>', re.DOTALL).findall(content)
	title = translation(40135)
	if itunesShowSubGenres:
		title = '[B]'+title+'[/B]'
	addAutoPlayDir(title, "0", "listItunesVideos", "", "", "browse")
	for genreID, type, title in match:
		title = cleanTitle(title)
		if 'class="top-level-genre"' in type:
			if itunesShowSubGenres:
				title = '[B]'+title+'[/B]'
			addAutoPlayDir(title, genreID, "listItunesVideos", "", "", "browse")
		elif itunesShowSubGenres:
			title = '     '+title
			addAutoPlayDir(title, genreID, "listItunesVideos", "", "", "browse")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def listItunesVideos(type, genreID, limit):
	if type == "play":
		musicVideos = []
		playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
		playlist.clear()
	url = "https://itunes.apple.com/"+iTunesRegion+"/rss/topsongs/limit=100"
	if genreID != "0":
		url += "/genre="+genreID
	url += "/explicit=true/json"
	content = cache(url, 1)
	content = json.loads(content)
	pos = 1
	for item in content['feed']['entry']:
		artist = item['im:artist']['label'].encode('utf-8')
		videoTitle = item['im:name']['label'].encode('utf-8')
		if " (" in videoTitle:
			videoTitle = videoTitle[:videoTitle.rfind(" (")]
		title = cleanTitle(artist+" - "+videoTitle)
		try:
			thumb = item['im:image'][2]['label'].replace("170x170-75.jpg","500x500-144.jpg")
		except:
			thumb = ""
		filtered = False
		for entry2 in blacklist:
			if entry2.strip().lower() and entry2.strip().lower() in title.lower():
				filtered = True
		if filtered:
			continue
		if type == "browse":
			addLink(title, title.replace(" - ", " "), "playYTByTitle", thumb)
		else:
			url = "plugin://"+addonID+"/?url="+urllib.quote_plus(title.replace(" - ", " "))+"&mode=playYTByTitle"
			musicVideos.append([title, url, thumb])
			if limit and int(limit)==pos:
				break
			pos += 1
	if type == "browse":
		xbmcplugin.endOfDirectory(pluginhandle)
		if forceView:
			xbmc.executebuiltin('Container.SetViewMode('+viewIDVideos+')')
	else:
		random.shuffle(musicVideos)
		for title, url, thumb in musicVideos:
			listitem = xbmcgui.ListItem(title, thumbnailImage=thumb)
			playlist.add(url, listitem)
		xbmc.Player().play(playlist)
	
	
def ocMain():
	addAutoPlayDir(translation(40139), urlMainOC+"/charts/singles-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40140), urlMainOC+"/charts/uk-top-40-singles-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40141), urlMainOC+"/charts/asian-download-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40142), urlMainOC+"/charts/singles-chart-update/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40143), urlMainOC+"/charts/singles-downloads-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40144), urlMainOC+"/charts/singles-sales-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40145), urlMainOC+"/charts/audio-streaming-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40146), urlMainOC+"/charts/vinyl-singles-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40147), urlMainOC+"/charts/scottish-singles-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40148), urlMainOC+"/charts/physical-singles-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40149), urlMainOC+"/charts/end-of-year-singles-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40150), urlMainOC+"/charts/classical-singles-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40151), urlMainOC+"/charts/dance-singles-chart/", "listOC", "", "", "browse")
	addAutoPlayDir(translation(40152), urlMainOC+"/charts/rock-and-metal-singles-chart/", "listOC", "", "", "browse")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def listOC(type, url, limit):
	if type == "play":
		musicVideos = []
		playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
		playlist.clear()
	content = cache(url, 1)
	spl=content.split('<div class="track">')
	pos = 1
	for i in range(1,len(spl),1):
		entry=spl[i]
		match1 = re.compile('<img src="(.*?)"', re.DOTALL).findall(entry)
		thumb = match1[0]
		if 'images-amazon' in thumb:
			thumb = thumb.replace("http://live.chartsdb.aws.occ.drawgroup.com/img/small?url=", "")
		else:
			thumb = thumb.replace("img/small", "img/big")
		videoTitle = re.findall('<a href=".+?">(.*?)</a>',entry,re.S)[0]
		artist = re.findall('<a href=".+?">(.*?)</a>',entry,re.S)[1]
		if "/" in artist:
			artist = artist[:artist.find("/")].strip()
		title = cleanTitle(artist+" - "+videoTitle)
		filtered = False
		for entry2 in blacklist:
			if entry2.strip().lower() and entry2.strip().lower() in title.lower():
				filtered = True
		if filtered:
			continue
		if type == "browse":
			addLink(title.title().replace(' Ft ', ' feat. '), title.replace(" - ", " "), "playYTByTitle", thumb)
		else:
			url = "plugin://"+addonID+"/?url="+urllib.quote_plus(title.replace(" - ", " "))+"&mode=playYTByTitle"
			musicVideos.append([title, url, thumb])
			if limit and int(limit)==pos:
				break
			pos += 1
	if type == "browse":
		xbmcplugin.endOfDirectory(pluginhandle)
		if forceView:
			xbmc.executebuiltin('Container.SetViewMode('+viewIDVideos+')')
	else:
		random.shuffle(musicVideos)
		for title, url, thumb in musicVideos:
			listitem = xbmcgui.ListItem(title, thumbnailImage=thumb)
			playlist.add(url, listitem)
		xbmc.Player().play(playlist)
	
	
def spotifyMain():
	addDir(translation(40153), "https://api.tunigo.com/v3/space/toplists?region="+spotifyRegion+"&page=0&per_page=50&platform=web", "listSpotifyPlaylists", "")
	addDir(translation(40154), "https://api.tunigo.com/v3/space/featured-playlists?region="+spotifyRegion+"&page=0&per_page=50&dt="+datetime.datetime.now().strftime("%Y-%m-%dT%H:%M").replace(":","%3A")+"%3A00&platform=web", "listSpotifyPlaylists", "")
	addDir(translation(40155), "https://api.tunigo.com/v3/space/genres?region="+spotifyRegion+"&per_page=1000&platform=web", "listSpotifyGenres", "")
	xbmcplugin.endOfDirectory(pluginhandle)
	
	
def listSpotifyGenres(url):
	content = cache(url, 30)
	content = json.loads(content)
	for item in content['items']:
		title = item['genre']['name'].encode('utf-8')
		if title.isupper():
			title = title.title()
		genreID = item['genre']['templateName']
		try:
			thumb = item['genre']['iconUrl']
		except:
			thumb = ""
		if not "top lists" in title.strip().lower():
			addDir(title, "https://api.tunigo.com/v3/space/"+genreID+"?region="+spotifyRegion+"&page=0&per_page=50&platform=web", "listSpotifyPlaylists", thumb)
	xbmcplugin.endOfDirectory(pluginhandle)
	if forceView:
		xbmc.executebuiltin('Container.SetViewMode('+viewIDGenres+')')
	
	
def listSpotifyPlaylists(url):
	content = cache(url, 1)
	content = json.loads(content)
	for item in content['items']:
		title = item['playlist']['title'].encode('utf-8')
		if title.isupper():
			title = title.title()
		description = item['playlist']['description'].encode('utf-8')
		uriUrl = item['playlist']['uri'].encode('utf-8')
		try:
			thumb = item['playlist']['image']
		except:
			thumb = ""
		addAutoPlayDir(title, uriUrl, "listSpotifyVideos", thumb, description, "browse")
	match = re.compile('page=(.+?)&per_page=(.+?)&', re.DOTALL).findall(url)
	currentPage = int(match[0][0])
	perPage = int(match[0][1])
	goNextPage = currentPage+1
	if goNextPage*perPage < content['totalItems']:
		addDir(translation(40201), url.replace("page="+str(currentPage),"page="+str(goNextPage)), "listSpotifyPlaylists", pic+'nextpage.png')
	xbmcplugin.endOfDirectory(pluginhandle)
	if forceView:
		xbmc.executebuiltin('Container.SetViewMode('+viewIDPlaylists+')')
	
	
def listSpotifyVideos(type, url, limit):
	musicVideos = []
	if type == "play":
		playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
		playlist.clear()
	#content = cache("https://open.spotify.com/embed?uri="+url, 1)
	content = cache("https://embed.spotify.com/?uri="+url, 1)
	if '<div class="ppbtn"></div>' in content:
		spl = content.split('music-paused item')
		x=0
	else:
		kurz_inhalt = content[content.find('<ul class="track-list">')+1:]
		kurz_inhalt = kurz_inhalt[:kurz_inhalt.find('<button id')]
		spl = kurz_inhalt.split('class="track-row"')
		x=1
	for i in range(1,len(spl),1):
		entry = spl[i]
		if x==0:
			videoTitle = re.compile('class="track-title.+?>(.*?)<', re.DOTALL).findall(entry)[0]
		else:
			videoTitle = re.compile('data-name="(.*?)"', re.DOTALL).findall(entry)[0]
		if x==0:
			artist = re.compile('class="artist.+?>(.*?)<', re.DOTALL).findall(entry)[0]
		else:
			artist = re.compile('data-artists="(.*?)"', re.DOTALL).findall(entry)[0]
		#videoTitle = videoTitle[videoTitle.find(".")+1:].strip()
		if " [" in videoTitle:
			videoTitle = videoTitle[:videoTitle.rfind(" [")]
		if " - " in videoTitle:
			firstTitle = videoTitle[:videoTitle.rfind(" - ")]
			secondTitle = videoTitle[videoTitle.find(" - ")+3:]
			secondTitle = ' ['+secondTitle+']'
			videoTitle = firstTitle+secondTitle
		if "," in artist:
			artist = artist[:artist.find(",")]
		if artist.islower():
			artist = artist.title()
		if artist == "":
			continue
		title = cleanTitle(artist+" - "+videoTitle)
		oTitle = title
		try:
			if x==0:
				thumb = re.compile('data-ca="(.*?)"', re.DOTALL).findall(entry)[0]
			else:
				thumb = re.findall('data-size-[0-9]+="(.*?)"',entry,re.S)[0]
		except:
			thumb = ""
		rank = re.compile('class="track-row-number">(.*?)</div>', re.DOTALL).findall(entry)[0]
		filtered = False
		for entry2 in blacklist:
			if entry2.strip().lower() and entry2.strip().lower() in title.lower():
				filtered = True
		if filtered:
			continue
		if type == "play":
			url = "plugin://"+addonID+"/?url="+urllib.quote_plus(oTitle.replace(" - ", " "))+"&mode=playYTByTitle"
		else:
			url = oTitle
		musicVideos.append([int(rank), title, url, thumb])
	musicVideos = sorted(musicVideos, key=itemgetter(0))
	if type == "browse":
		for rank, title, url, thumb in musicVideos:
			name = '[B][COLOR chartreuse]'+str(rank)+' •  [/COLOR]'+title+'[/B]'
			addLink(name, url.replace(" - ", " "), "playYTByTitle", thumb)
		xbmcplugin.endOfDirectory(pluginhandle)
		if forceView:
			xbmc.executebuiltin('Container.SetViewMode('+viewIDVideos+')')
	else:
		if limit:
			musicVideos = musicVideos[:int(limit)]
		random.shuffle(musicVideos)
		for rank, title, url, thumb in musicVideos:
			listitem = xbmcgui.ListItem(title, thumbnailImage=thumb)
			playlist.add(url, listitem)
		xbmc.Player().play(playlist)
	
	
def playYTByTitle(title):
	try:
		finalUrl = ""
		youtubeID = getYoutubeId(title)
		pluginID = 'plugin.video.youtube'
		plugin = xbmcaddon.Addon(id=pluginID)
		finalUrl = 'plugin://'+plugin.getAddonInfo('id')+'/play/?video_id='+youtubeID
		listitem = xbmcgui.ListItem(path=finalUrl)
		xbmcplugin.setResolvedUrl(pluginhandle, True, listitem)
		xbmc.sleep(2000)
		if infoEnabled and xbmc.Player().isPlaying():
			showInfo()
	except:
		pass
	
	
def getYoutubeId(title):
	title = urllib.quote_plus(title.lower())
	content = cache("https://www.googleapis.com/youtube/v3/search?part=snippet&max-results=1&order=relevance&q=%s&key=%s" %(title,api_key), 1)
	spl = content.split('"id": ')
	videoID = ""
	for i in range(1,len(spl),1):
		entry = spl[i]
		videoBest = ""
		videoBest = re.findall('"videoId": "(.*?)"',content,re.S)[0]
		oTitle = re.findall('"title": "(.*?)"',content,re.S)[0]
		if "audio" in oTitle.strip().lower():
			videoBest = re.findall('"videoId": "(.*?)"',content,re.S)[1]
			oTitle = re.findall('"title": "(.*?)"',content,re.S)[1]
			if "audio" in oTitle.strip().lower():
				videoBest = re.findall('"videoId": "(.*?)"',content,re.S)[2]
				oTitle = re.findall('"title": "(.*?)"',content,re.S)[2]
				if "audio" in oTitle.strip().lower():
					videoBest = re.findall('"videoId": "(.*?)"',content,re.S)[0]
		videoID = videoBest
		#content = json.loads(content)
		#for item in content['items']:
		#videoID = item['id']['videoId']
		#title = item['snippet']['title'].encode('utf-8')
	return videoID
	
	
def queueVideo(url, name, thumb):
	playlist = xbmc.PlayList(xbmc.PLAYLIST_VIDEO)
	listitem = xbmcgui.ListItem(name, iconImage=thumb, thumbnailImage=thumb)
	playlist.add(url, listitem)
	
	
def makeRequest(url,headers=False):
	req = urllib2.Request(url)
	if headers:
		for key in headers:
			req.add_header(key, headers[key])
	else:
		req.add_header('User-Agent', 'Mozilla/5.0 (Windows NT 10.0; WOW64; rv:49.0) Gecko/20100101 Firefox/32.0')
		req.add_header('Accept-Encoding','gzip,deflate')
	response = urllib2.urlopen(req)
	if response.info().get('Content-Encoding') == 'gzip':
		buf = StringIO(response.read())
		f = gzip.GzipFile(fileobj=buf)
		link = f.read()
		f.close()
	else:
		link = response.read()
		response.close()
	return link
	
	
def cache(url, duration=0):
	cacheFile = os.path.join(cachePath, (''.join(c for c in unicode(url, 'utf-8') if c not in '/\\:?"*|<>')).strip())
	if len(cacheFile) > 255:
		cacheFile = cacheFile[:255]
	if os.path.exists(cacheFile) and duration!=0 and (time.time()-os.path.getmtime(cacheFile) < 60*60*24*duration):
		fh = xbmcvfs.File(cacheFile, 'r')
		content = fh.read()
		fh.close()
	else:
		content = makeRequest(url)
		fh = xbmcvfs.File(cacheFile, 'w')
		fh.write(content)
		fh.close()
	return content
	
	
def showInfo():
	count = 0
	while not xbmc.Player().isPlaying():
		xbmc.sleep(200)
		if count == 50:
			break
		count += 1
	xbmc.sleep(infoDelay*1000)
	if (not xbmc.abortRequested) and infoType == "0":
		xbmc.executebuiltin('XBMC.ActivateWindow(12901)')
		xbmc.sleep(infoDuration*1000)
		xbmc.executebuiltin('XBMC.ActivateWindow(12005)')
	elif (not xbmc.abortRequested) and infoType == "1":
		TOP = translation(40202)
		relTitle = xbmc.getInfoLabel('Player.Title').decode("utf-8").encode("utf-8").replace(",", " ").replace('–', '-').replace(" Ft ", " feat. ").replace(" ft ", " feat. ").replace("ft.", "feat.").replace("Featuring", "feat.")
		if relTitle.isupper() or relTitle.islower():
			relTitle = relTitle.title()
		runTime = xbmc.getInfoLabel('Player.Duration')
		picture = xbmc.getInfoImage('Player.Art(thumb)')
		if relTitle:
			xbmc.executebuiltin('Notification(%s,%s,%d,%s)' %(TOP, relTitle+"[COLOR blue]  * "+runTime+" *[/COLOR]", infoDuration*1000, picture))
	else:
		pass
	
	
def cleanTitle(title):
	title = title.replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&").replace("&#39;", "'").replace("&#039;", "'").replace("&quot;", "\"").replace("&szlig;", "ß").replace("&ndash;", "-").replace("#", "").replace('–', '-')
	title = title.replace("&#x00c4", "Ä").replace("&#x00e4", "ä").replace("&#x00d6", "Ö").replace("&#x00f6", "ö").replace("&#x00dc", "Ü").replace("&#x00fc", "ü").replace("&#x00df", "ß")
	title = title.replace("&Auml;", "Ä").replace("&Ouml;", "Ö").replace("&Uuml;", "Ü").replace("&auml;", "ä").replace("&ouml;", "ö").replace("&uuml;", "ü")
	title = title.replace("&agrave;", "à").replace("&aacute;", "á").replace("&egrave;", "è").replace("&eacute;", "é").replace("&igrave;", "ì").replace("&iacute;", "í")
	title = title.replace("&ograve;", "ò").replace("&oacute;", "ó").replace("&ugrave;", "ù").replace("&uacute;", "ú").replace(" ft ", " feat. ").replace("ft.", "feat.").replace("Feat.", "feat.").replace("Featuring", "feat.")
	title = title.replace("\\'", "'").replace("&x27;", "'").replace("&iexcl;", "¡").replace("&raquo;", "»").replace("&laquo;", "«")
	title = title.strip()
	return title
	
	
def parameters_string_to_dict(parameters):
	paramDict = {}
	if parameters:
		paramPairs = parameters[1:].split("&")
		for paramsPair in paramPairs:
			paramSplits = paramsPair.split('=')
			if (len(paramSplits)) == 2:
				paramDict[paramSplits[0]] = paramSplits[1]
	return paramDict
	
	
def addLink(name, url, mode, iconimage):
	u = sys.argv[0]+"?url="+urllib.quote_plus(url)+"&mode="+str(mode)
	ok = True
	liz = xbmcgui.ListItem(name, iconImage="DefaultAudio.png", thumbnailImage=iconimage)
	liz.setInfo(type="Video", infoLabels={"Title": name})
	liz.setProperty('IsPlayable', 'true')
	if useThumbAsFanart:
		liz.setProperty("fanart_image", fanart)
	xbmcplugin.setContent(int(sys.argv[1]), 'musicvideos')
	entries = []
	entries.append((translation(40203),'RunPlugin(plugin://'+addonID+'/?mode=queueVideo&url='+urllib.quote_plus(u)+'&name='+urllib.quote_plus(name)+'&thumb='+urllib.quote_plus(iconimage)+')',))
	liz.addContextMenuItems(entries)
	ok = xbmcplugin.addDirectoryItem(handle=int(sys.argv[1]), url=u, listitem=liz)
	return ok
	
	
def addDir(name, url, mode, iconimage="", description="", type="", limit=""):
	u = sys.argv[0]+"?url="+urllib.quote_plus(url)+"&mode="+str(mode)+"&type="+str(type)+"&limit="+str(limit)
	ok = True
	liz = xbmcgui.ListItem(name, iconImage="DefaultMusicVideos.png", thumbnailImage=iconimage)
	liz.setInfo(type="Video", infoLabels={"Title": name, "Plot": description})
	if useThumbAsFanart:
		liz.setProperty("fanart_image", fanart)
	ok = xbmcplugin.addDirectoryItem(handle=int(sys.argv[1]), url=u, listitem=liz, isFolder=True)
	return ok
	
	
def addAutoPlayDir(name, url, mode, iconimage="", description="", type="", limit=""):
	u = sys.argv[0]+"?url="+urllib.quote_plus(url)+"&mode="+str(mode)+"&type="+str(type)+"&limit="+str(limit)
	ok = True
	liz = xbmcgui.ListItem(name, iconImage="DefaultMusicVideos.png", thumbnailImage=iconimage)
	liz.setInfo(type="Video", infoLabels={"Title": name, "Plot": description})
	if useThumbAsFanart:
		liz.setProperty("fanart_image", fanart)
	entries = []
	entries.append((translation(40170), 'RunPlugin(plugin://'+addonID+'/?mode='+str(mode)+'&url='+urllib.quote_plus(url)+'&type=play&limit=)',))
	entries.append((translation(40171), 'RunPlugin(plugin://'+addonID+'/?mode='+str(mode)+'&url='+urllib.quote_plus(url)+'&type=play&limit=10)',))
	entries.append((translation(40172), 'RunPlugin(plugin://'+addonID+'/?mode='+str(mode)+'&url='+urllib.quote_plus(url)+'&type=play&limit=20)',))
	entries.append((translation(40173), 'RunPlugin(plugin://'+addonID+'/?mode='+str(mode)+'&url='+urllib.quote_plus(url)+'&type=play&limit=30)',))
	entries.append((translation(40174), 'RunPlugin(plugin://'+addonID+'/?mode='+str(mode)+'&url='+urllib.quote_plus(url)+'&type=play&limit=40)',))
	entries.append((translation(40175), 'RunPlugin(plugin://'+addonID+'/?mode='+str(mode)+'&url='+urllib.quote_plus(url)+'&type=play&limit=50)',))
	liz.addContextMenuItems(entries)
	ok = xbmcplugin.addDirectoryItem(handle=int(sys.argv[1]), url=u, listitem=liz, isFolder=True)
	return ok
	
	
params = parameters_string_to_dict(sys.argv[2])
name = urllib.unquote_plus(params.get('name', ''))
url = urllib.unquote_plus(params.get('url', ''))
mode = urllib.unquote_plus(params.get('mode', ''))
thumb = urllib.unquote_plus(params.get('thumb', ''))
type = urllib.unquote_plus(params.get('type', ''))
limit = urllib.unquote_plus(params.get('limit', ''))
	
	
if mode == 'billboardMain':
	billboardMain()
elif mode == 'listBillboardArchiveMain':
	listBillboardArchiveMain()
elif mode == 'listBillboardArchive':
	listBillboardArchive(url)
elif mode == 'listBillboardArchiveVideos':
	listBillboardArchiveVideos(type, url, limit)
elif mode == 'listBillboardChartsTypes':
	listBillboardChartsTypes(url)
elif mode == 'listBillboardCharts':
	listBillboardCharts(type, url, limit)
elif mode == 'listBillboardChartsNew':
	listBillboardChartsNew(type, url, limit)
elif mode == 'bpMain':
	bpMain()
elif mode == 'listBP':
	listBP(type, url, limit)
elif mode == 'hypemMain':
	hypemMain()
elif mode == 'listHypem':
	listHypem(type, url, limit)
elif mode == 'listTimeMachine':
	listTimeMachine()
elif mode == 'itunesMain':
	itunesMain()
elif mode == 'listItunesVideos':
	listItunesVideos(type, url, limit)
elif mode == 'ocMain':
	ocMain()
elif mode == 'listOC':
	listOC(type, url, limit)
elif mode == 'spotifyMain':
	spotifyMain()
elif mode == 'listSpotifyGenres':
	listSpotifyGenres(url)
elif mode == 'listSpotifyPlaylists':
	listSpotifyPlaylists(url)
elif mode == 'listSpotifyVideos':
	listSpotifyVideos(type, url, limit)
elif mode == 'playYTByTitle':
	playYTByTitle(url)
elif mode == 'queueVideo':
	queueVideo(url, name, thumb)
else:
	index()
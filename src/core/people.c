/* The passengers: people with a name, an age, a job and a bit of their life they tell on the way, in German
   and English. A regular picks the story up where it stopped last time (jobs.c). */
#include "people.h"
#include "cars.h"

const TsPerson ts_people[] = {
    {"Lena", 27, "Tierärztin", "vet", {
        {"Ich bin Tierärztin. Heute hatte ich ein Pony mit Bauchweh.", "I'm a vet. Today I had a pony with a tummy ache."},
        {"Die Pferde auf den Höfen hier sind total verwöhnt.", "The horses on the farms around here are totally spoiled."},
        {"Mein eigener Kater heißt Diesel. Er hasst Autos.", "My own cat is called Diesel. He hates cars."},
        {"Nachts werde ich oft angerufen, wenn ein Fohlen kommt.", "I often get called at night when a foal is coming."},
        {"Eigentlich wollte ich mal Rennfahrerin werden. Dann kamen die Tiere.", "I actually wanted to be a racing driver. Then the animals came along."},
        {"Wenn du mal einen Hund findest, bring ihn zu mir.", "If you ever find a stray dog, bring it to me."},
    }},
    {"Marco", 34, "Koch", "chef", {
        {"Ich koche in einem kleinen Restaurant in Castelletto.", "I cook in a little restaurant in Castelletto."},
        {"Heute Abend gibt's Risotto mit Steinpilzen. Komm vorbei!", "Tonight it's porcini risotto. Drop by!"},
        {"Der Trick bei Pasta: Nudelwasser so salzig wie das Meer.", "The trick with pasta: water as salty as the sea."},
        {"Meine Nonna kocht trotzdem besser als ich. Sag's keinem.", "My nonna still cooks better than me. Don't tell anyone."},
        {"Ich fahr zum Markt, die besten Tomaten sind früh weg.", "I'm going to the market, the best tomatoes go early."},
        {"Zehn Stunden in der Küche, und trotzdem koch ich zu Hause noch.", "Ten hours in the kitchen, and I still cook at home."},
    }},
    {"Sophie", 22, "Studentin", "student", {
        {"Ich studiere Architektur in Lyon und mach hier ein Praktikum.", "I study architecture in Lyon and I'm doing an internship here."},
        {"Die alten Steinhäuser hier sind mein Lieblingsthema.", "The old stone houses here are my favourite subject."},
        {"Meine Abschlussarbeit hängt mir echt nach.", "My thesis is really weighing on me."},
        {"Ich zeichne überall. Sogar auf Servietten.", "I sketch everywhere. Even on napkins."},
        {"Am Wochenende geh ich mit Freunden zelten.", "At the weekend I'm going camping with friends."},
        {"Wenn ich fertig bin, bau ich ein Haus mit Blick aufs Meer.", "When I'm done, I'll build a house with a sea view."},
    }},
    {"Luca", 19, "Mechaniker-Azubi", "apprentice mechanic", {
        {"Ich mach eine Lehre als Mechaniker. Ich hör jedes Geräusch vom Motor.", "I'm training as a mechanic. I can hear every sound the engine makes."},
        {"Gestern hab ich zum ersten Mal ein Getriebe allein zerlegt.", "Yesterday I took a gearbox apart on my own for the first time."},
        {"Mein Meister sagt, ich hab Öl im Blut.", "My boss says I've got oil in my blood."},
        {"Ich spar auf einen alten Fiat. Den bau ich selbst auf.", "I'm saving up for an old Fiat. I'll restore it myself."},
        {"Dein Auto klingt übrigens gesund. Gutes Zeichen.", "Your car sounds healthy, by the way. Good sign."},
        {"Irgendwann will ich in einem Rennteam schrauben.", "Some day I want to wrench for a race team."},
    }},
    {"Emma", 41, "Lehrerin", "teacher", {
        {"Ich bin Grundschullehrerin. Sommerferien, endlich!", "I'm a primary school teacher. Summer holidays, finally!"},
        {"Meine Klasse hat mir zum Abschied ein Bild gemalt. Ich musste weinen.", "My class painted me a picture to say goodbye. I cried."},
        {"Dreißig Kinder sind lauter als jedes Festival.", "Thirty kids are louder than any festival."},
        {"Ich korrigiere sogar im Urlaub noch Hefte. Schlimm, oder?", "I'm even marking exercise books on holiday. Terrible, right?"},
        {"Mein Sohn will auch zum Festival. Er ist zwölf. Nein.", "My son wants to come to the festival too. He's twelve. No."},
        {"Nächstes Jahr mache ich mit der Klasse einen Ausflug hierher.", "Next year I'll bring my class here on a trip."},
    }},
    {"Jonas", 30, "Fotograf", "photographer", {
        {"Ich fotografiere für ein Automagazin. Das Licht hier ist perfekt.", "I shoot for a car magazine. The light here is perfect."},
        {"Die beste Zeit ist kurz vor Sonnenuntergang. Goldene Stunde.", "The best time is just before sunset. Golden hour."},
        {"Ich hab heute fünftausend Fotos gemacht. Drei sind gut.", "I took five thousand photos today. Three are good."},
        {"Mein Objektiv kostet mehr als mein erstes Auto.", "My lens cost more than my first car."},
        {"Darf ich dich mal beim Fahren fotografieren? Später vielleicht.", "Can I photograph you driving some time? Maybe later."},
        {"Mein Traum ist ein Bild auf dem Cover. Nur eins.", "My dream is one picture on the cover. Just one."},
    }},
    {"Giulia", 36, "Winzerin", "winemaker", {
        {"Meiner Familie gehört ein Weingut bei Montellino.", "My family owns a vineyard near Montellino."},
        {"Die Lese fängt in drei Wochen an. Dann schlaf ich kaum.", "The harvest starts in three weeks. Then I barely sleep."},
        {"Unser Rotwein hat letztes Jahr einen Preis gewonnen.", "Our red won a prize last year."},
        {"Mein Vater redet mit den Reben. Ehrlich.", "My father talks to the vines. Honestly."},
        {"Der Hagel im Frühling hat uns fast alles gekostet.", "The hail in spring almost cost us everything."},
        {"Komm im Herbst vorbei, dann gibt's eine Flasche für dich.", "Come by in autumn and there's a bottle for you."},
    }},
    {"Pierre", 63, "Rentner", "pensioner", {
        {"Ich war vierzig Jahre Briefträger in Nizza.", "I was a postman in Nice for forty years."},
        {"Damals kannte ich jede Straße und jeden Hund.", "Back then I knew every street and every dog."},
        {"Jetzt spiele ich Boule. Jeden Tag um vier.", "Now I play boules. Every day at four."},
        {"Meine Frau sagt, ich rede zu viel. Sie hat recht.", "My wife says I talk too much. She's right."},
        {"Früher gab's hier keine Festivals, nur Schafe.", "There were no festivals here back then, only sheep."},
        {"Mein Enkel hat mir eure Fahrdienst-App gezeigt. Moderne Zeiten!", "My grandson showed me your ride app. Modern times!"},
    }},
    {"Chloé", 25, "Sängerin", "singer", {
        {"Ich singe heute Abend auf der kleinen Bühne am Festival.", "I'm singing on the small stage at the festival tonight."},
        {"Ich bin so nervös. Erzähl mir was Lustiges.", "I'm so nervous. Tell me something funny."},
        {"Mein erstes Konzert war in einer Bar vor sieben Leuten.", "My first gig was in a bar in front of seven people."},
        {"Ich schreibe meine Lieder auf Französisch und Englisch.", "I write my songs in French and English."},
        {"Meine Stimme braucht Tee mit Honig. Viel Honig.", "My voice needs tea with honey. Lots of honey."},
        {"Wenn du mich im Radio hörst, sag allen, dass du mich gefahren hast.", "If you hear me on the radio, tell everyone you drove me."},
    }},
    {"Matteo", 29, "Barkeeper", "bartender", {
        {"Ich mixe Drinks in der Lounge am Festival.", "I mix drinks in the lounge at the festival."},
        {"Letzte Nacht hab ich fünfhundert Mojitos gemacht.", "Last night I made five hundred mojitos."},
        {"Die Leute erzählen dem Barkeeper alles. Wirklich alles.", "People tell the bartender everything. Really everything."},
        {"Mein Spezialdrink heißt Horizon Sunset. Mit Orange.", "My signature drink is the Horizon Sunset. With orange."},
        {"Ich schlaf meistens bis mittags. Heute nicht.", "I usually sleep till noon. Not today."},
        {"Irgendwann mach ich meine eigene Bar auf, direkt am Strand.", "Some day I'll open my own bar, right on the beach."},
    }},
    {"Hannah", 33, "Ärztin", "doctor", {
        {"Ich bin Ärztin im Sanitätszelt vom Festival.", "I'm a doctor in the festival's first-aid tent."},
        {"Die meisten kommen wegen Sonnenbrand. Trink Wasser, wirklich.", "Most people come in with sunburn. Drink water, really."},
        {"Gestern hatte einer seinen Zeh im Auspuff verbrannt. Frag nicht.", "Yesterday someone burned their toe on an exhaust. Don't ask."},
        {"Nach zwölf Stunden Schicht schlaf ich überall ein.", "After a twelve-hour shift I can fall asleep anywhere."},
        {"Ich jogge morgens am Meer. Das hält mich wach.", "I jog by the sea in the mornings. It keeps me awake."},
        {"Wenn du dich gut fühlst, sieh mich bitte nie bei der Arbeit.", "If you're feeling fine, please never see me at work."},
    }},
    {"Noah", 24, "Surflehrer", "surf instructor", {
        {"Ich bin Surflehrer unten an der Küste.", "I'm a surf instructor down on the coast."},
        {"Heute waren die Wellen klein, aber die Schüler hatten Spaß.", "The waves were small today, but the students had fun."},
        {"Einmal hat mich ein Delfin begleitet. Bestes Gefühl ever.", "A dolphin swam with me once. Best feeling ever."},
        {"Im Winter geh ich in die Berge snowboarden.", "In winter I go snowboarding in the mountains."},
        {"Meine Haare sind vom Salz ganz hart. Sorry.", "My hair's all stiff from the salt. Sorry."},
        {"Komm mal vorbei, die erste Stunde ist umsonst.", "Come by sometime, the first lesson is free."},
    }},
    {"Elena", 52, "Bürgermeisterin", "mayor", {
        {"Ich bin Bürgermeisterin von Sisteron. Bitte sag's nicht weiter.", "I'm the mayor of Sisteron. Please keep it to yourself."},
        {"Das Festival bringt uns viele Gäste. Und viel Lärm.", "The festival brings us lots of visitors. And lots of noise."},
        {"Diese Straße hier lassen wir nächstes Jahr neu machen.", "We're having this road resurfaced next year."},
        {"Jeder will mit mir über Parkplätze reden. Jeder.", "Everyone wants to talk to me about parking. Everyone."},
        {"Ich fahre bewusst mit dir statt mit dem Dienstwagen.", "I'm riding with you on purpose instead of the official car."},
        {"Wenn du Ideen für die Stadt hast, schreib mir.", "If you've got ideas for the town, write to me."},
    }},
    {"Felix", 38, "Software-Entwickler", "software developer", {
        {"Ich bin Programmierer und arbeite eigentlich im Homeoffice.", "I'm a programmer and normally work from home."},
        {"Ich hab mein Laptop dabei. Nur für den Notfall. Natürlich.", "I've got my laptop with me. Just for emergencies. Of course."},
        {"Gestern hab ich einen Fehler gesucht, sechs Stunden. Es war ein Komma.", "Yesterday I hunted a bug for six hours. It was a comma."},
        {"Ich programmiere nebenbei ein kleines Rennspiel.", "On the side I'm programming a little racing game."},
        {"Meine Freundin hat mir verboten, im Urlaub E-Mails zu lesen.", "My girlfriend banned me from reading email on holiday."},
        {"Ich hab grad eine geschrieben. Verrat mich nicht.", "I just wrote one. Don't tell on me."},
    }},
    {"Camille", 31, "Floristin", "florist", {
        {"Ich mache die Blumen für eine Hochzeit morgen.", "I'm doing the flowers for a wedding tomorrow."},
        {"Die Braut will Lavendel, überall Lavendel.", "The bride wants lavender, lavender everywhere."},
        {"Mein Laden in Nizza riecht immer nach Frühling.", "My shop in Nice always smells like spring."},
        {"Sonnenblumen sind ehrlich gesagt meine Lieblinge.", "Sunflowers are honestly my favourites."},
        {"Um vier Uhr morgens bin ich schon auf dem Großmarkt.", "At four in the morning I'm already at the wholesale market."},
        {"Wenn du mal jemanden beeindrucken willst, komm zu mir.", "If you ever want to impress someone, come to me."},
    }},
    {"Antonio", 47, "Bauer", "farmer", {
        {"Ich hab Olivenbäume, gut dreihundert Stück.", "I've got olive trees, a good three hundred of them."},
        {"Manche sind älter als mein Großvater.", "Some are older than my grandfather."},
        {"Das Öl vom letzten Jahr war das beste seit langem.", "Last year's oil was the best in ages."},
        {"Ich fahr sonst Traktor. Das hier ist viel bequemer.", "I usually drive a tractor. This is much comfier."},
        {"Mein Sohn will den Hof nicht übernehmen. Er will DJ werden.", "My son doesn't want to take over the farm. He wants to be a DJ."},
        {"Vielleicht hat er ja recht. Wer weiß.", "Maybe he's right. Who knows."},
    }},
    {"Mia", 17, "Schülerin", "school student", {
        {"Ich bin zum ersten Mal allein auf dem Festival!", "I'm at the festival on my own for the first time!"},
        {"Meine Mutter hat mir dreißig Nachrichten geschrieben.", "My mum has sent me thirty messages."},
        {"Nächstes Jahr mach ich den Führerschein.", "Next year I'm getting my licence."},
        {"Ich will später Rennstrecken designen. Ehrlich.", "Later I want to design race tracks. Honestly."},
        {"Kannst du mir einen Tipp fürs Fahren geben?", "Can you give me a driving tip?"},
        {"Ich hab heute zwei Bands gesehen, die ich nie kannte. Beide super.", "Today I saw two bands I'd never heard of. Both great."},
    }},
    {"Oskar", 70, "Ex-Rennfahrer", "former racing driver", {
        {"Ich bin früher Rallyes gefahren. In den Siebzigern.", "I used to drive rallies. In the seventies."},
        {"Die Bergstraßen hier kenn ich mit geschlossenen Augen.", "I know the mountain roads here with my eyes closed."},
        {"Damals gab's keine Airbags. Nur Mut.", "Back then there were no airbags. Just nerve."},
        {"Mein schlimmster Unfall war in einer Kurve wie der da vorne.", "My worst crash was on a bend like the one up ahead."},
        {"Du hast eine ruhige Hand. Das sieht man sofort.", "You've got a steady hand. You can see it right away."},
        {"Heute fahr ich nur noch Rad. Die Knie, weißt du.", "These days I only cycle. The knees, you know."},
    }},
    {"Sara", 28, "Influencerin", "influencer", {
        {"Ich mach Reisevideos. Darf ich dich kurz filmen? Nur von hinten.", "I make travel videos. Can I film you quickly? Just from behind."},
        {"Meine Follower lieben Autos. Und Sonnenuntergänge.", "My followers love cars. And sunsets."},
        {"Ich hab heute schon drei Mal das gleiche Croissant fotografiert.", "I've photographed the same croissant three times today."},
        {"Ehrlich gesagt ist das ganz schön anstrengend.", "Honestly, it's pretty exhausting."},
        {"Am liebsten würde ich mal einen Tag ohne Handy sein.", "I'd love to spend a day without my phone."},
        {"Okay, vielleicht morgen. Oder übermorgen.", "Okay, maybe tomorrow. Or the day after."},
    }},
    {"Tom", 45, "Pilot", "pilot", {
        {"Ich bin Pilot. Morgen fliege ich nach Lissabon.", "I'm a pilot. Tomorrow I'm flying to Lisbon."},
        {"Von oben sieht die Küste hier aus wie eine Postkarte.", "From above, this coast looks like a postcard."},
        {"Die Leute klatschen immer noch bei der Landung. Ich mag das.", "People still clap on landing. I like that."},
        {"Autofahren ist für mich fast entspannender als Fliegen.", "Driving is almost more relaxing for me than flying."},
        {"Meine Kinder glauben, ich wohne im Flugzeug.", "My kids think I live in the plane."},
        {"Turbulenzen? Hast du gerade nicht gemacht. Gut gemacht.", "Turbulence? You didn't make any just now. Well done."},
    }},
    {"Lucie", 35, "Hebamme", "midwife", {
        {"Ich bin Hebamme. Heute Nacht kamen zwei Babys.", "I'm a midwife. Two babies came last night."},
        {"Eins davon hieß nach einem Rennfahrer. Ayrton.", "One of them was named after a racing driver. Ayrton."},
        {"Ich trinke Kaffee wie andere Leute Wasser.", "I drink coffee the way other people drink water."},
        {"Das schönste Geräusch ist der erste Schrei.", "The most beautiful sound is the first cry."},
        {"Mein Handy ist immer laut. Falls es klingelt: sorry.", "My phone's always on loud. If it rings: sorry."},
        {"Wenn du mal Papa oder Mama wirst: ruhig bleiben. Wie beim Fahren.", "If you ever become a parent: stay calm. Like driving."},
    }},
    {"Ben", 26, "DJ", "DJ", {
        {"Ich leg heute um Mitternacht auf. Hauptbühne!", "I'm DJing at midnight today. Main stage!"},
        {"Ich hab drei Stunden Musik, aber nur zwei Stunden Zeit.", "I've got three hours of music but only two hours of time."},
        {"Mein Vater ist Olivenbauer. Er versteht meinen Job nicht.", "My dad's an olive farmer. He doesn't get my job."},
        {"Ich hör Beats sogar im Motorgeräusch.", "I even hear beats in the engine noise."},
        {"Wenn du Lust hast, komm. Ich setz dich auf die Liste.", "If you fancy it, come. I'll put you on the list."},
        {"Mein erstes Set war auf einer Geburtstagsfeier. Drei Gäste.", "My first set was at a birthday party. Three guests."},
    }},
    {"Ines", 39, "Polizistin", "police officer", {
        {"Ich bin Polizistin, hab aber frei. Keine Sorge.", "I'm a police officer, but I'm off duty. Don't worry."},
        {"Na ja, ich schau trotzdem auf den Tacho. Gewohnheit.", "Well, I'm still watching the speedo. Habit."},
        {"Beim Festival hab ich letzte Woche einen Hund wiedergefunden.", "At the festival last week I found a lost dog."},
        {"Die meisten Unfälle passieren in dieser Kurve da.", "Most accidents happen on that bend there."},
        {"Privat fahre ich übrigens einen sehr langsamen Kleinwagen.", "Privately I drive a very slow little car, by the way."},
        {"Gute Fahrt bisher. Ehrlich, ich würd dich nicht anhalten.", "Good driving so far. Honestly, I wouldn't pull you over."},
    }},
    {"Yusuf", 32, "Bäcker", "baker", {
        {"Ich bin Bäcker. Mein Tag fängt um drei Uhr nachts an.", "I'm a baker. My day starts at three in the morning."},
        {"Riechst du das? Ich hab Brot in der Tasche. Frisch.", "Smell that? I've got bread in my bag. Fresh."},
        {"Das Festival kauft jeden Morgen tausend Croissants bei uns.", "The festival buys a thousand croissants from us every morning."},
        {"Mein Sauerteig ist zwölf Jahre alt. Er heißt Bruno.", "My sourdough starter is twelve years old. It's called Bruno."},
        {"Nimm dir eins, wirklich. Ich hab genug.", "Take one, really. I've got plenty."},
        {"Abends schlaf ich um acht. Ich bin ein langweiliger Mensch.", "I go to bed at eight. I'm a boring person."},
    }},
    {"Clara", 44, "Journalistin", "journalist", {
        {"Ich schreibe über das Festival für eine Zeitung in Berlin.", "I'm writing about the festival for a Berlin newspaper."},
        {"Darf ich dich zitieren? Wie ist das Leben als Fahrer hier?", "Can I quote you? What's life like driving here?"},
        {"Die beste Geschichte findest du nie auf der Bühne.", "You never find the best story on the stage."},
        {"Mein Redakteur will den Text bis Mitternacht.", "My editor wants the piece by midnight."},
        {"Ich hab heute mit einem Schafhirten geredet. Zwei Stunden.", "I talked to a shepherd today. For two hours."},
        {"Vielleicht schreib ich über Horizon Transit. Mal sehen.", "Maybe I'll write about Horizon Transit. We'll see."},
    }},
    {"David", 50, "Busfahrer", "bus driver", {
        {"Ich fahr selbst Bus, Linie 2. Heute hab ich frei.", "I drive a bus myself, line 2. Day off today."},
        {"Schön, mal hinten zu sitzen. Ungewohnt.", "Nice to sit in the back for once. Feels odd."},
        {"Die Kurve am Hafen ist mit dem Bus ein Albtraum.", "The bend by the harbour is a nightmare in a bus."},
        {"Meine Stammgäste bringen mir manchmal Kuchen.", "My regulars sometimes bring me cake."},
        {"Du bremst schön früh. Das gefällt mir.", "You brake nice and early. I like that."},
        {"Wenn du mal Bus fahren willst: Wir suchen immer Leute.", "If you ever want to drive a bus: we're always hiring."},
    }},
    {"Aylin", 23, "Tänzerin", "dancer", {
        {"Ich tanze in der Show am Festival. Jeden Abend.", "I dance in the show at the festival. Every night."},
        {"Meine Füße tun so weh, ich könnte heulen.", "My feet hurt so much I could cry."},
        {"Wir proben die Choreo seit drei Monaten.", "We've been rehearsing the choreography for three months."},
        {"Morgen tanzen wir zum ersten Mal bei Sonnenuntergang.", "Tomorrow we dance at sunset for the first time."},
        {"Früher hab ich in meinem Zimmer vor dem Spiegel getanzt.", "I used to dance in my room in front of the mirror."},
        {"Komm zur Show. Reihe eins, ich winke dir.", "Come to the show. Front row, I'll wave to you."},
    }},
    {"Henri", 58, "Kapitän", "ship captain", {
        {"Ich fahre die Fähre zwischen Nizza und Korsika.", "I captain the ferry between Nice and Corsica."},
        {"Auf dem Meer gibt's keine Kurven. Nur Wellen.", "There are no bends at sea. Only waves."},
        {"Einmal hatten wir ein Pferd an Bord. Ganz ruhig, das Tier.", "Once we had a horse on board. Very calm animal."},
        {"An Land werde ich manchmal seekrank. Komisch, oder?", "On land I sometimes get seasick. Funny, isn't it?"},
        {"Mein Großvater war auch Kapitän. Und sein Vater.", "My grandfather was a captain too. And his father."},
        {"Wenn du mal nach Korsika willst: Kaffee geht auf mich.", "If you ever want to go to Corsica: coffee's on me."},
    }},
    {"Nora", 30, "Kletterin", "climber", {
        {"Ich klettere in den Felsen bei Sisteron.", "I climb on the cliffs near Sisteron."},
        {"Heute hab ich eine Route geschafft, an der ich ein Jahr hing.", "Today I finished a route I'd been stuck on for a year."},
        {"Meine Hände sind voller Kreide, sorry für den Sitz.", "My hands are covered in chalk, sorry about the seat."},
        {"Höhenangst hab ich nur im Auto. Komisch.", "I'm only afraid of heights in cars. Weird."},
        {"Mein Freund sichert mich immer. Er sagt, ich klettere wie eine Katze.", "My boyfriend always belays me. He says I climb like a cat."},
        {"Morgen ruh ich mich aus. Vielleicht.", "Tomorrow I'll rest. Maybe."},
    }},
    {"Kenji", 37, "Ingenieur", "engineer", {
        {"Ich bin Ingenieur und baue Bremsen für Sportwagen.", "I'm an engineer and I design brakes for sports cars."},
        {"Ich höre gerade, wie deine Bremsen arbeiten. Gut.", "I'm listening to how your brakes are working. Good."},
        {"Ich bin aus Osaka, aber ich lebe seit fünf Jahren in Italien.", "I'm from Osaka, but I've lived in Italy for five years."},
        {"Pasta ist gut. Aber ich vermisse Ramen.", "Pasta is good. But I miss ramen."},
        {"Am Wochenende teste ich Autos auf der Rennstrecke.", "At weekends I test cars on the race track."},
        {"Physik ist überall. Auch in dieser Kurve.", "Physics is everywhere. Even in this bend."},
    }},
    {"Julia", 46, "Anwältin", "lawyer", {
        {"Ich bin Anwältin. Und ja, ich lese alles Kleingedruckte.", "I'm a lawyer. And yes, I read all the small print."},
        {"Ich hab heute einen Streit um einen Olivenbaum geschlichtet.", "Today I settled a dispute about an olive tree."},
        {"Die Leute streiten über die verrücktesten Sachen.", "People argue about the craziest things."},
        {"Im Urlaub bin ich keine Anwältin. Theoretisch.", "On holiday I'm not a lawyer. In theory."},
        {"Mein Mann kocht, ich verhandle. Gute Aufteilung.", "My husband cooks, I negotiate. Good division of labour."},
        {"Wenn dich jemand ärgert, hier ist meine Karte. Spaß. Halb.", "If anyone bothers you, here's my card. Joking. Half joking."},
    }},
    {"Max", 21, "Fahrradkurier", "bike courier", {
        {"Ich bin Fahrradkurier. Heute lass ich mich mal fahren.", "I'm a bike courier. Today someone else does the riding."},
        {"Ich fahr sonst an Autos wie deinem vorbei. Im Stau.", "Normally I ride past cars like yours. In traffic."},
        {"Meine Beine haben heute achtzig Kilometer gemacht.", "My legs did eighty kilometres today."},
        {"Mein Rad hat einen Platten. Deshalb bin ich hier.", "My bike's got a flat. That's why I'm here."},
        {"Die Abfahrt von den Bergen ist mit dem Rad der Wahnsinn.", "Coming down from the mountains on a bike is insane."},
        {"Ich glaub, ich kauf mir doch mal ein Auto. Irgendwann.", "I think I'll buy a car after all. Some day."},
    }},
    {"Rosa", 67, "Oma auf Reisen", "travelling grandma", {
        {"Ich besuche meine Enkel. Ich hab Kekse für alle dabei.", "I'm visiting my grandkids. I've brought biscuits for everyone."},
        {"Mein erster Urlaub hier war 1975. Mit einem Moped.", "My first holiday here was in 1975. On a moped."},
        {"Damals war mein Mann noch so schlank wie du.", "Back then my husband was as slim as you."},
        {"Ich hab meinen Enkeln das Kartenspielen beigebracht. Jetzt schlagen sie mich.", "I taught my grandkids cards. Now they beat me."},
        {"Möchtest du einen Keks? Die sind mit Mandeln.", "Would you like a biscuit? They've got almonds."},
        {"Du erinnerst mich an meinen Neffen. Der fährt auch so ordentlich.", "You remind me of my nephew. He drives carefully too."},
    }},
    {"Leon", 35, "Feuerwehrmann", "firefighter", {
        {"Ich bin bei der Feuerwehr. Im Sommer ist hier viel los.", "I'm a firefighter. Summer's busy around here."},
        {"Die Waldbrände sind das Schlimmste. Trockene Hügel, viel Wind.", "Forest fires are the worst. Dry hills, lots of wind."},
        {"Heute hab ich eine Katze vom Baum geholt. Klischee, aber wahr.", "Today I got a cat down from a tree. Cliché, but true."},
        {"Bei uns kocht jeden Tag ein anderer. Heute ich. Oje.", "At the station someone else cooks every day. Today it's me. Uh-oh."},
        {"Wenn du Rauch siehst, sofort anrufen. Versprochen?", "If you see smoke, call straight away. Promise?"},
        {"Ich mag ruhige Tage. Die sind selten.", "I like quiet days. They're rare."},
    }},
    {"Valentina", 27, "Modedesignerin", "fashion designer", {
        {"Ich entwerfe Kleider. Die Show ist nächste Woche in Mailand.", "I design clothes. The show's in Milan next week."},
        {"Die Farben hier inspirieren mich. Dieses Blau vom Meer!", "The colours here inspire me. That sea blue!"},
        {"Ich hab eine Tasche voller Stoffproben dabei.", "I've got a bag full of fabric samples."},
        {"Mein erstes Kleid hab ich mit zwölf aus einem Vorhang genäht.", "I sewed my first dress at twelve, from a curtain."},
        {"Dein Auto hat eine tolle Farbe. Das notier ich mir.", "Your car's a great colour. I'm writing that down."},
        {"Wenn die Kollektion klappt, bin ich glücklich. Wenn nicht, auch.", "If the collection works, I'm happy. If not, also happy."},
    }},
    {"Erik", 42, "Förster", "forester", {
        {"Ich bin Förster in den Wäldern über Sisteron.", "I'm a forester in the woods above Sisteron."},
        {"Gestern hab ich einen Luchs gesehen. Ganz selten!", "Yesterday I saw a lynx. Really rare!"},
        {"Ich zähl die Bäume. Na ja, nicht alle.", "I count the trees. Well, not all of them."},
        {"Im Wald ist es so still. Hier ist mir zu viel los.", "It's so quiet in the forest. There's too much going on here."},
        {"Mein Hund kennt jeden Weg. Besser als ich.", "My dog knows every path. Better than me."},
        {"Fahr vorsichtig in den Bergen. Da springen Rehe raus.", "Drive carefully in the mountains. Deer jump out there."},
    }},
    {"Amélie", 33, "Konditorin", "pastry chef", {
        {"Ich backe Torten. Heute eine Hochzeitstorte mit fünf Stockwerken.", "I make cakes. Today a five-tier wedding cake."},
        {"Ich hab Angst, dass sie schmilzt. Es ist so heiß.", "I'm scared it'll melt. It's so hot."},
        {"Mein Geheimnis ist Zitrone. Überall ein bisschen Zitrone.", "My secret is lemon. A little lemon in everything."},
        {"Ich hab in Paris gelernt, aber hier ist es schöner.", "I trained in Paris, but it's nicer here."},
        {"Probier mal, ich hab Makronen dabei.", "Have a taste, I've got macarons with me."},
        {"Wer Süßes macht, ist selten schlecht gelaunt. Meistens.", "People who make sweets are rarely grumpy. Usually."},
    }},
    {"Paul", 55, "Weinhändler", "wine merchant", {
        {"Ich kaufe Wein für Restaurants in Deutschland ein.", "I buy wine for restaurants in Germany."},
        {"Heute hab ich zwölf Weingüter besucht. Nur probiert, nicht getrunken.", "I visited twelve vineyards today. Tasted, not drunk."},
        {"Na gut, ein bisschen getrunken.", "Okay, drunk a little."},
        {"Der beste Wein hier kommt von einer kleinen Familie bei Montellino.", "The best wine here comes from a little family near Montellino."},
        {"Mein Kofferraum klirrt, wenn ich zu schnell fahre. Du nicht. Gut.", "My boot clinks when I drive too fast. You don't. Good."},
        {"Am Ende des Tages zählt nur: Schmeckt es?", "At the end of the day, all that counts is: does it taste good?"},
    }},
    {"Zoe", 20, "Festival-Helferin", "festival volunteer", {
        {"Ich bin Helferin am Festival. Ich verteile Armbänder.", "I volunteer at the festival. I hand out wristbands."},
        {"Heute hab ich viertausend Armbänder verteilt. Mein Arm!", "I handed out four thousand wristbands today. My arm!"},
        {"Dafür darf ich alle Konzerte umsonst sehen.", "In return I get to see every concert for free."},
        {"Ich hab heute einen berühmten Rennfahrer getroffen. Glaub ich.", "I met a famous racing driver today. I think."},
        {"Mein Zelt steht direkt neben der Bühne. Schlafen? Nein.", "My tent is right next to the stage. Sleep? No."},
        {"Nächstes Jahr bin ich wieder dabei. Ganz sicher.", "I'll be back next year. Definitely."},
    }},
    {"Gabriel", 48, "Priester", "priest", {
        {"Ich bin Pfarrer in einem kleinen Dorf in den Bergen.", "I'm the priest in a little mountain village."},
        {"Heute ist eine Taufe. Das Baby heißt Enzo.", "There's a christening today. The baby is called Enzo."},
        {"Manchmal predige ich übers Autofahren. Geduld ist eine Tugend.", "Sometimes I preach about driving. Patience is a virtue."},
        {"Unsere Glocke ist dreihundert Jahre alt und ziemlich laut.", "Our bell is three hundred years old and pretty loud."},
        {"Ich spiel abends Gitarre. Schlecht, aber gern.", "I play guitar in the evenings. Badly, but gladly."},
        {"Gott segne deine Bremsen. Und dich natürlich.", "God bless your brakes. And you, of course."},
    }},
};
const unsigned ts_people_count = sizeof(ts_people) / sizeof(ts_people[0]);

/* what kind of car each one likes and doesn't like (TS_GROUP_), in the order of ts_people */
const signed char ts_people_taste[][2] = {
    {TS_GROUP_OFFROAD, TS_GROUP_SUPER}, /* Lena */
    {TS_GROUP_CLASSIC, TS_GROUP_OFFROAD}, /* Marco */
    {TS_GROUP_CLASSIC, TS_GROUP_OFFROAD}, /* Sophie */
    {TS_GROUP_SPORTS, TS_GROUP_SMALL}, /* Luca */
    {TS_GROUP_SALOON, TS_GROUP_SUPER}, /* Emma */
    {TS_GROUP_CLASSIC, TS_GROUP_SMALL}, /* Jonas */
    {TS_GROUP_OFFROAD, TS_GROUP_SUPER}, /* Giulia */
    {TS_GROUP_CLASSIC, TS_GROUP_SUPER}, /* Pierre */
    {TS_GROUP_SUPER, TS_GROUP_OFFROAD}, /* Chloé */
    {TS_GROUP_SPORTS, TS_GROUP_SALOON}, /* Matteo */
    {TS_GROUP_SALOON, TS_GROUP_SPORTS}, /* Hannah */
    {TS_GROUP_OFFROAD, TS_GROUP_SALOON}, /* Noah */
    {TS_GROUP_SALOON, TS_GROUP_SUPER}, /* Elena */
    {TS_GROUP_SALOON, TS_GROUP_CLASSIC}, /* Felix */
    {TS_GROUP_SMALL, TS_GROUP_OFFROAD}, /* Camille */
    {TS_GROUP_OFFROAD, TS_GROUP_SUPER}, /* Antonio */
    {TS_GROUP_SUPER, TS_GROUP_SMALL}, /* Mia */
    {TS_GROUP_CLASSIC, TS_GROUP_OFFROAD}, /* Oskar */
    {TS_GROUP_SUPER, TS_GROUP_SMALL}, /* Sara */
    {TS_GROUP_SALOON, TS_GROUP_OFFROAD}, /* Tom */
    {TS_GROUP_SALOON, TS_GROUP_SUPER}, /* Lucie */
    {TS_GROUP_SUPER, TS_GROUP_SALOON}, /* Ben */
    {TS_GROUP_SMALL, TS_GROUP_SUPER}, /* Ines */
    {TS_GROUP_SMALL, TS_GROUP_SUPER}, /* Yusuf */
    {TS_GROUP_CLASSIC, TS_GROUP_OFFROAD}, /* Clara */
    {TS_GROUP_SALOON, TS_GROUP_SPORTS}, /* David */
    {TS_GROUP_SPORTS, TS_GROUP_OFFROAD}, /* Aylin */
    {TS_GROUP_CLASSIC, TS_GROUP_SUPER}, /* Henri */
    {TS_GROUP_OFFROAD, TS_GROUP_SALOON}, /* Nora */
    {TS_GROUP_SPORTS, TS_GROUP_OFFROAD}, /* Kenji */
    {TS_GROUP_SALOON, TS_GROUP_HATCH}, /* Julia */
    {TS_GROUP_HATCH, TS_GROUP_OFFROAD}, /* Max */
    {TS_GROUP_SALOON, TS_GROUP_SUPER}, /* Rosa */
    {TS_GROUP_OFFROAD, TS_GROUP_SMALL}, /* Leon */
    {TS_GROUP_CLASSIC, TS_GROUP_OFFROAD}, /* Valentina */
    {TS_GROUP_OFFROAD, TS_GROUP_SUPER}, /* Erik */
    {TS_GROUP_SALOON, TS_GROUP_SPORTS}, /* Amélie */
    {TS_GROUP_SALOON, TS_GROUP_SUPER}, /* Paul */
    {TS_GROUP_HATCH, TS_GROUP_SALOON}, /* Zoe */
    {TS_GROUP_SMALL, TS_GROUP_SUPER}, /* Gabriel */
};

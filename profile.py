import geni.portal as portal
import geni.rspec.pg as rspec
import geni.rspec.igext as IG

pc = portal.Context()
request = rspec.Request()

tourDescription = "This profile provides a baseline Ubuntu 14.4 image"

#
# Setup the Tour info with the above description and instructions.
#  
tour = IG.Tour()
tour.Description(IG.Tour.TEXT,tourDescription)
request.addTour(tour)

node = request.RawPC("basenode")
node.disk_image = "urn:publicid:IDN+emulab.net+image+emulab-ops:UBUNTU14-64-STD"

node.addService(rspec.Execute(shell="/bin/sh",
                              command="touch test.txt"))   

# Print the RSpec to the enclosing page.
portal.context.printRequestRSpec(request)
